/*
 * pc_disc_manifest_none.c: the whole-disc check's empty reference list (2026-11-02; pc_disc_check.c). A link
 * that has the generated list (port/tools/firstlink.py: build-pc/generated/pc_disc_manifest.c, from
 * tools/local/port/discmanifest.py) never pulls this archive member in; every other program the platform library
 * goes into (orreport.exe, the tests) gets this one, and the check then logs that it was skipped.
 */
#include <stdint.h>

typedef struct { const char* path; uint32_t size; uint8_t sha1[20]; } PcDiscFile;
const uint32_t pc_disc_fst_entries = 0;
const uint32_t pc_disc_file_count = 0;
const PcDiscFile pc_disc_files[1] = {{0}};
const char pc_disc_manifest_id[] = "";
