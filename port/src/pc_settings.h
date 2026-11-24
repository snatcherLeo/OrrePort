/* pc_settings.h: the port's settings file, <exe folder>/user/settings.ini (pc_settings.c). */
#ifndef PC_SETTINGS_H
#define PC_SETTINGS_H

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

void pc_settings_load(const char* user_folder); /* the folder ends with a separator; a missing file means defaults */
int pc_settings_int(const char* key, int fallback);
const char* pc_settings_string(const char* key, const char* fallback); /* valid until the next set / load */
void pc_settings_set_int(const char* key, int value);
void pc_settings_set_string(const char* key, const char* value);
void pc_settings_save(void);
/* *Added 2026-11-20 (step 3.7, the logs carry every setting):* every change is logged by the setters above; when it
 * happened ("game frame N", or "hub +N.N s" before the game runs) as text, the game frame's source (the game program
 * sets it at start-up; without it the hub's seconds), and every key in the file, for the start-up snapshot */
void pc_settings_when(char* out, size_t cap);
void pc_settings_set_frame_source(unsigned (*frame)(void));
int pc_settings_count(void);
const char* pc_settings_key_at(int i);
const char* pc_settings_value_at(int i);

#ifdef __cplusplus
}
#endif

#endif
