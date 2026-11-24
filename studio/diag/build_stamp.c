/* build_stamp.c: the build's identity (build_stamp.h says why). The only file that includes the generated header, so a
 * new stamp recompiles just this one. Plain C; every platform. */
#include "build_stamp.h"

#if defined(__has_include)
#if __has_include("build_stamp_gen.h")
#include "build_stamp_gen.h"
#endif
#endif
#ifndef DIAG_BUILD_STAMP
#define DIAG_BUILD_STAMP "build stamp unknown"
#endif

const char* diag_build_stamp(void) { return DIAG_BUILD_STAMP; }
