#ifndef GAME_GS_MODEL_H
#define GAME_GS_MODEL_H

#include "dolphin/types.h"

struct GSbound;
struct GSjobjDesc;
struct GSlight;
struct GSmodel;
struct GSmodelResource;
struct GSvec;
struct HSDJObj;

/* GS vector API. */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void set__5GSvecFfff(struct GSvec* vec, f32 x, f32 y, f32 z);
#endif

/* GS model construction and animation API. */
struct GSmodel* _modelLoad(struct GSmodelResource* resource,
                           struct GSjobjDesc* joint, void* boundAnim);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void GSmodelSetAnimRate(struct GSmodel* model, f32 rate);
#endif
void GSmodelSetTexAnimIndex(struct GSmodel* model, u32 index);
void GSmodelSetTexAnimRate(struct GSmodel* model, f32 rate);
void GSmodelSetTexAnimType(struct GSmodel* model, u32 type);
void GSmodelRecalculateBound(struct GSmodel* model);
void GSmodelApplyAll(void* unused);

/* Shadow state API. Partial model/light layouts remain private to the owner. */
void GSmodelFreeAllShadowTextures(void);
void GSmodelSetShadowBoundExpansion(u32 extent, u32 state);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void GSmodelSetShadowLight(struct GSmodel* model, struct GSlight* light);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void GSmodelSetShadowSurface(struct GSmodel* model, s32 count,
                             const void* vertices);
#endif

/* HSD services used while constructing a GS model. */
struct HSDJObj* HSD_JObjLoadJoint(struct GSjobjDesc* joint);
void HSD_JObjAddNext(struct HSDJObj* jobj, struct HSDJObj* next);
void fn_80190E60(struct GSbound* bound);
void fn_8019146C(struct GSbound* bound, const struct GSvec* scale);
void fn_80191474(struct GSbound* bound, const struct GSvec* rotation);
void fn_8019147C(struct GSbound* bound, const struct GSvec* position);
void fn_8019F1C4(struct HSDJObj* jobj, s32* vertexCount,
                 s32* polygonCount);

#endif /* GAME_GS_MODEL_H */
