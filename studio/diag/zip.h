/*
 * zip: a zip file with its contents stored as they are (no compression), written whole (2026-11-22; 4.3 part 3,
 * Report a problem's zip).
 *
 * Why: a report is one file a tester can paste into Discord. Stored entries keep the writer small and the files
 * readable by any unzip tool; reports are a few MB, so compression isn't worth a dependency. The API has the shape of
 * OrrePort's pc_zip_write (port/src/pc_zip.h) so the two programs can share one writer in the diagnostics library: this
 * one is plain C over stdio and malloc (OrrePort's goes through SDL, which Studio doesn't link). Main decides which
 * one stays (asked 2026-11-22).
 *
 * Written to "<path>.part" first, then renamed over <path>, so a crash never leaves half a zip behind.
 * Platforms: plain C; the rename is MoveFileExW on Windows (the path is UTF-8, converted), rename() elsewhere.
 */
#ifndef ORRE_DIAG_ZIP_H
#define ORRE_DIAG_ZIP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DiagZipFile {
    const char* name; /* the name inside the zip (UTF-8, "/" between folders) */
    const void* data;
    size_t size;
} DiagZipFile;

/* writes the files into a new zip at path (UTF-8); 1 on success, 0 when it couldn't be written (nothing left behind) */
int diag_zip_write(const char* path, const DiagZipFile* files, int count);

#ifdef __cplusplus
}
#endif

#endif
