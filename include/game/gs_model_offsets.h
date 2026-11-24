/**
 * @file gs_model_offsets.h
 * @brief Raw offsets into a GSmodel for code that reads it through a byte pointer (effect_visual.c's surf / sea
 *        effects read the model's render joint tree at +0x08 on the GameCube). On the PC the GSmodel record is native
 *        (8-byte pointers), so the field moves; the PC value is checked against offsetof in tests/pc_hsd/main_gfx.c.
 *        Kept apart from gs_model_struct.h because those units define their own conflicting helper types.
 */
#ifndef GAME_GS_MODEL_OFFSETS_H
#define GAME_GS_MODEL_OFFSETS_H

#ifdef TARGET_PC
#define GSMODEL_OFFSET_RENDER_JOBJ 0x10
#else
#define GSMODEL_OFFSET_RENDER_JOBJ 0x8
#endif

#endif /* GAME_GS_MODEL_OFFSETS_H */
