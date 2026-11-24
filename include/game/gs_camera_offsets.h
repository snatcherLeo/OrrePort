#ifndef GAME_GS_CAMERA_OFFSETS_H
#define GAME_GS_CAMERA_OFFSETS_H
/* Raw offsets into the render camera (GSRenderCamera, gs_render_util.h) that some callers read directly (
 * 2026-11-04; port-gaps 61). The record holds pointers (desc, cobj), 8 bytes on the PC, so its later fields sit
 * further in there; gs_render_util.h checks these against the struct on the PC. */
#ifdef TARGET_PC
#define GS_CAMERA_OFF_INTEREST 0x108 /* the point the camera looks at */
#else
#define GS_CAMERA_OFF_INTEREST 0x100
#endif
#endif
