/**
 * GSmodelEnableEnvMap, 0x800E61BC - 0x800E638C (one-function carve).
 * Returns early when the first material already has the env-map extension
 * (the inlined GSmodelIsEnvMapEnabled), then builds the material list on
 * the first reference (modelInitMaterialList, XD _modelInitMaterialList) and
 * enables the env map on every material.
 */
#define GSMODEL_SUFFIX_ISOLATED
#include "src/game/gs_model_main_suffix_800E4AC0.c"
#include "game/gs_model_material_internal.h"

/* The definition's real order (gs_material_exact_800DF1B8.c): integers first, then the float. A declaration with
 * (material, blend, texture, matrix, light) matched the GameCube's registers but passes the wrong values on x64. */
void GSmaterialSetEnvMapParams(void* material, void* mode, void* type, f32 blend, void* texture); /* mode / type are integers */
void GSmaterialEnableExtension(void* material, s32 mode);

/* RULE-EXCEPTION(title-path): single-use inline copy of GSmodelIsEnvMapEnabled
 * (0x800E5FAC, its own carve) so this carve can expand it as retail does -
 * see docs/RULE_EXCEPTIONS.md */
static inline u8 modelIsEnvMapEnabled(GSmodel* model)
{
    void* material;

    if (model->materialCount == 0) {
        return 0;
    }

    material = model->materialList->materials[0];
    if (material == NULL) {
        return 0;
    }

    if (GSmaterialGetEnabledExtensions(material) & 4) {
        return 1;
    }
    return 0;
}

void GSmodelEnableEnvMap(GSmodel* model, void* mode, void* type,
                         void* texture, f32 blend) /* mode / type: integers (see GSmaterialSetEnvMapParams) */
{
    s32 i;
    s32 count;
    void** materials;

    if (modelIsEnvMapEnabled(model)) {
        return;
    }

    if (model->modulationRefCount == 0) {
        count = modelInitMaterialList(model);
    } else {
        count = model->materialCount;
    }
    model->modulationRefCount++;
    materials = (void**)model->materialList;
    for (i = 0; i < count; i++, materials++) {
        if (*materials != NULL) {
            GSmaterialSetEnvMapParams(*materials, mode, type, blend,
                                      texture);
            GSmaterialEnableExtension(*materials, 4);
        }
    }
}
