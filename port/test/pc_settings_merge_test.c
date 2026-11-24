/*
 * pc_settings_merge_test.c: settings.ini with two writers (OrrePort and 2026-11-23, from Studio's
 * question). Studio changes update_check / update_auto_download in the file while OrrePort is open; OrrePort's next
 * save must keep Studio's change, both when OrrePort never touched the key and when it had set (and saved) it earlier.
 * OrrePort's own unsaved change to a key still wins. Run: pc_settings_merge_test <scratch folder ending in a slash>.
 * Plain C and SDL: every OS the same.
 */
#include "../src/pc_settings.h"

#include <stdio.h>
#include <string.h>

static char s_file[1200];
static int s_fails;

static void check(int ok, const char* what) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    s_fails += !ok;
}

/* another program's write: the whole file, as Studio's temporary-file-and-rename does */
static void studio_writes(const char* body) {
    FILE* f = fopen(s_file, "w");
    if (f == NULL) return;
    fputs(body, f);
    fclose(f);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: pc_settings_merge_test <scratch folder ending in a slash>\n");
        return 2;
    }
    snprintf(s_file, sizeof s_file, "%ssettings.ini", argv[1]);
    studio_writes("update_check = 1\nupdate_auto_download = 0\ntheme = 0\n");
    pc_settings_load(argv[1]);

    /* 1: a key OrrePort never set: Studio's change survives OrrePort's save */
    studio_writes("update_check = 1\nupdate_auto_download = 1\ntheme = 0\n");
    pc_settings_set_int("theme", 1);
    pc_settings_save();
    pc_settings_load(argv[1]);
    check(pc_settings_int("update_auto_download", -1) == 1, "Studio's change to a key OrrePort never set is kept");
    check(pc_settings_int("theme", -1) == 1, "OrrePort's own change is written too");

    /* 2: a key OrrePort set and saved earlier, changed by Studio afterwards: Studio's (newer) change is kept */
    pc_settings_set_int("update_check", 1);
    pc_settings_save();
    studio_writes("update_check = 0\nupdate_auto_download = 1\ntheme = 1\n");
    pc_settings_set_int("theme", 0);
    pc_settings_save();
    pc_settings_load(argv[1]);
    check(pc_settings_int("update_check", -1) == 0, "Studio's later change wins over OrrePort's earlier, saved one");

    /* 3: both change the same key and OrrePort's change is the newer one (not saved yet): OrrePort's wins */
    studio_writes("update_check = 0\nupdate_auto_download = 0\ntheme = 0\n");
    pc_settings_set_int("update_auto_download", 1);
    pc_settings_save();
    pc_settings_load(argv[1]);
    check(pc_settings_int("update_auto_download", -1) == 1, "OrrePort's newer, unsaved change wins");

    printf("%s\n", s_fails ? "SOME CHECKS FAILED" : "all passed");
    return s_fails ? 1 : 0;
}
