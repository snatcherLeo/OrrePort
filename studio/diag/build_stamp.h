/*
 * build_stamp: which build this is (2026-11-22; 4.3 part 3, the logging list's item 1).
 *
 * Why: every log and every report has to say exactly which build it came from, so a problem report can be matched to
 * its code. The line: "<program> <MAIN BUILD | TEST BUILD (folder)> - <commit>[ +changes] (<branch>) built <date time>",
 * the same shape as OrrePort's stamp (port/tools/firstlink.py), generated at build time by diag/build_stamp.cmake.
 *
 * Shared diagnostics library: plain C, no app types; the same on every platform (the stamp is made by CMake and git).
 */
#ifndef ORRE_DIAG_BUILD_STAMP_H
#define ORRE_DIAG_BUILD_STAMP_H

#ifdef __cplusplus
extern "C" {
#endif

/* the build's stamp line (never NULL; "unknown" parts when git wasn't there at build time) */
const char* diag_build_stamp(void);

#ifdef __cplusplus
}
#endif

#endif
