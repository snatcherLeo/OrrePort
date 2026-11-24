/*
 * pc_zip.h: zip files with their contents stored as they are (no compression): written whole, and read back
 * (2026-11-15; first users: the one-file .orresave for travel, 2.4 part 5; pc_zip.c says why).
 * Shared with Studio (2026-11-22, 4.3 part 3): no SDL and no OrrePort types, so Studio compiles this file as it is
 * (as it does pc_crash_win32.c); one writer for both programs' report zips. Paths are UTF-8 on every system.
 * Platforms: plain C; Windows converts paths to UTF-16 inside (pc_zip.c says why); Linux and macOS use them as they are.
 */
#ifndef PC_ZIP_H
#define PC_ZIP_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PcZipFile {
    const char* name; /* the name inside the zip (UTF-8, "/" between folders) */
    const void* data;
    size_t size;
} PcZipFile;

/* writes the files into a new zip at path (whole, then moved into place, so a crash leaves no half file); 1 on success */
int pc_zip_write(const char* path, const PcZipFile* files, int count);

/* a file inside a zip held in memory: a pointer into `zip` and its size, or NULL when it isn't there or isn't stored as
 * it is (OrrePort's own zips always are) */
const void* pc_zip_find(const void* zip, size_t zip_size, const char* name, size_t* size_out);

/* 1 when the bytes start like a zip ("PK\3\4") */
int pc_zip_is_zip(const void* data, size_t size);

/* fopen for a UTF-8 path on every system (on Windows through _wfopen, shared for reading and writing like fopen, so a
 * log another handle is still writing can be read); for the files a report gathers */
FILE* pc_fopen_utf8(const char* path, const char* mode);

/* sets a file's last-change time to now (UTF-8 path); 1 on success. For a disc moved into ROM\ or ROM Other Regions\:
 * a move keeps the file's old time, and the shared "which disc" rule takes the newest (4.4, 2026-11-23), so the disc
 * just added must be the newest. Studio does the same after its copy and move. */
int pc_file_touch_utf8(const char* path);

#ifdef __cplusplus
}
#endif

#endif
