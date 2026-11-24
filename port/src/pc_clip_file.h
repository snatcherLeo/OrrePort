/*
 * pc_clip_file.h: a file put on the clipboard as a file, so Ctrl+V in a chat (Discord) or a folder attaches it
 * (2026-11-22).
 *
 * Why: Report a problem copies its zip after saving (snatcherLeo, 2026-11-22, decided with C for 4.3 part 3: "It's only the
 * one report, so it's harmless right?"), so a tester's habit is the same in OrrePort and Studio: save, then paste the
 * report where reports are shared. File Explorer still opens on the zip, for dragging it instead.
 *
 * Shared with Studio: plain C, no SDL and no OrrePort types (Studio compiles this file as it is, like pc_zip.c).
 * Platforms: Windows puts a file drop on the clipboard (CF_HDROP, plus "Preferred DropEffect" = copy so a folder paste
 * copies rather than moves). Linux and macOS: not written yet; returns 0 there, and the caller then
 * shows its "saved" line without "copied". Linux would offer text/uri-list (SDL_SetClipboardData can carry a MIME type);
 * macOS an NSPasteboard file URL (likely, both).
 */
#ifndef PC_CLIP_FILE_H
#define PC_CLIP_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Puts the file at `utf8_path` (forward or back slashes) on the clipboard as a file; 1 on success. Replaces whatever the
 * clipboard held. Call it only for an action the user took (never in automated runs: it would overwrite the clipboard
 * of whoever uses the PC). */
int pc_clipboard_set_file(const char* utf8_path);

#ifdef __cplusplus
}
#endif

#endif
