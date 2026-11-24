/*
 * pc_layout_test.c: the UI-per-ratio layout table's reader (port/src/pc_layout.c) on sample rows, and on the shipped
 * files when given their folder.
 *
 * Why: the table is the base of the widescreen UI work (docs/port/widescreen-plan.md section 6); nothing reads it at
 * draw time yet, so this test is what proves the format, the lookups and the "no row = 4:3 place, centred" rule before
 * the offset hooks rely on it.
 *
 * Run: build-pc/pc_layout_test.exe [<ui folder>/]   (with the folder, also reads layout/16x9.ini and 16x10.ini and
 * checks they are valid: every line a comment, a section or a known key; 0 problems)
 * Exit code 0 = every check passed. Plain C, no SDL: every OS.
 */
#include "pc_layout.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int s_fails, s_checks;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        ++s_checks;                                                                                                    \
        if (!(cond)) {                                                                                                 \
            ++s_fails;                                                                                                 \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                                                             \
        }                                                                                                              \
    } while (0)

static int near(float a, float b) { return fabsf(a - b) < 1e-4f; }

static const char kSample[] =
    "\xEF\xBB\xBF; sample rows (a byte-order mark first, as Windows editors write)\n"
    "# another comment\n"
    "\n"
    "[window.0013]\n"
    "anchor = right\n"
    "offset = -12, 4\n"
    "note = the battle command menu; keep it by the right edge\n"
    "[Menu.0013.Item.0911]\r\n"
    "anchor=left\r\n"
    "offset = 8 0\r\n"
    "[code.fade]\n"
    "anchor = stretch\n"
    "[effect.sunaarashi_all.0]\n"
    "span = full\n"
    "note = sand fills the wide picture; a ';' inside a note stays\n"
    "[art.title_bg]\n"
    "anchor = keep43\n"
    "border = #102030\n"
    "[window.0050]\n"
    "anchor = centre\n"
    "[window.0013]\n"          /* the same section again: replaces the first whole */
    "anchor = right\n"
    "offset = -20\n"           /* no y: read as -20, 0 (a problem logged) */
    "[bad id!]\n"              /* a bad id: its keys left out */
    "anchor = left\n"
    "[window.0060]\n"
    "anchor = sideways\n"      /* a bad value: kept centre */
    "colour = red\n"           /* an unknown key */
    "span = half\n";           /* a bad span */

int main(int argc, char** argv) {
    PcLayoutRow row;
    const PcLayoutRow* found;
    int rows;

    /* the sample */
    pc_layout_clear();
    rows = pc_layout_load_text(kSample, sizeof(kSample) - 1, "sample");
    printf("sample: %d rows read, %d held, %d problems\n", rows, pc_layout_count(), pc_layout_problems());
    CHECK(rows == 8);                  /* window.0013 twice, the menu item, fade, effect, art, 0050, 0060 */
    CHECK(pc_layout_count() == 7);     /* window.0013 replaced, not added */
    CHECK(pc_layout_problems() == 5);  /* no y, bad id, bad anchor, unknown key, bad span */

    found = pc_layout_find("window.0013");
    CHECK(found != NULL);
    if (found != NULL) {
        CHECK(found->anchor == PC_LAYOUT_RIGHT);
        CHECK(near(found->offsetX, -20.0f) && near(found->offsetY, 0.0f));
        CHECK(found->note[0] == '\0'); /* the replacing section had no note: replaced whole */
        CHECK(found->fromFile == 1);
    }
    found = pc_layout_find("MENU.0013.ITEM.0911"); /* ids are read and looked up in lower case */
    CHECK(found != NULL && found->anchor == PC_LAYOUT_LEFT && near(found->offsetX, 8.0f) && near(found->offsetY, 0.0f));
    found = pc_layout_find("code.fade");
    CHECK(found != NULL && found->anchor == PC_LAYOUT_STRETCH);
    found = pc_layout_find("effect.sunaarashi_all.0");
    CHECK(found != NULL && found->span == PC_LAYOUT_SPAN_FULL && found->anchor == PC_LAYOUT_CENTRE);
    CHECK(found != NULL && strcmp(found->note, "sand fills the wide picture; a ';' inside a note stays") == 0);
    found = pc_layout_find("art.title_bg");
    CHECK(found != NULL && found->anchor == PC_LAYOUT_KEEP43 && strcmp(found->border, "#102030") == 0);
    found = pc_layout_find("window.0060");
    CHECK(found != NULL && found->anchor == PC_LAYOUT_CENTRE && found->span == PC_LAYOUT_SPAN_DEFAULT);
    CHECK(pc_layout_find("bad id!") == NULL);

    /* no row: the 4:3 place, centred */
    CHECK(pc_layout_find("window.9999") == NULL);
    row = pc_layout_lookup("window.9999");
    CHECK(row.fromFile == 0 && row.anchor == PC_LAYOUT_CENTRE && near(row.offsetX, 0.0f) && near(row.offsetY, 0.0f) &&
          row.span == PC_LAYOUT_SPAN_DEFAULT && strcmp(row.id, "window.9999") == 0);
    CHECK(pc_layout_find(NULL) == NULL);
    CHECK(pc_layout_find("") == NULL);

    /* many rows: the map grows and every row is still found */
    {
        char text[64 * 1200];
        size_t n = 0;
        int i, all = 1;
        pc_layout_clear();
        for (i = 0; i < 1000; i++) {
            n += (size_t)snprintf(text + n, sizeof(text) - n, "[menu.%04x.item.%04x]\noffset = %d, %d\n", i % 37, i, i,
                                  -i);
        }
        rows = pc_layout_load_text(text, n, "many");
        CHECK(rows == 1000 && pc_layout_count() == 1000 && pc_layout_problems() == 0);
        for (i = 0; i < 1000; i++) {
            char id[64];
            snprintf(id, sizeof(id), "menu.%04x.item.%04x", i % 37, i);
            found = pc_layout_find(id);
            if (found == NULL || !near(found->offsetX, (float)i) || !near(found->offsetY, (float)-i)) {
                all = 0;
            }
        }
        CHECK(all);
        printf("many: %d rows, all found: %s\n", pc_layout_count(), all ? "yes" : "NO");
    }

    /* a later file replaces sections (a mod over the shipped file) */
    pc_layout_clear();
    {
        static const char shipped[] = "[window.0013]\nanchor = right\n[window.0014]\nanchor = left\n";
        static const char mod[] = "[window.0013]\nanchor = keep43\n";
        pc_layout_load_text(shipped, strlen(shipped), "shipped");
        pc_layout_load_text(mod, strlen(mod), "mod");
    }
    found = pc_layout_find("window.0013");
    CHECK(found != NULL && found->anchor == PC_LAYOUT_KEEP43);
    found = pc_layout_find("window.0014");
    CHECK(found != NULL && found->anchor == PC_LAYOUT_LEFT);

    /* the ratio names */
    CHECK(pc_layout_file_for_ratio("4x3") == NULL);
    CHECK(strcmp(pc_layout_file_for_ratio("16x9"), "16x9.ini") == 0);
    CHECK(strcmp(pc_layout_file_for_ratio("16x10"), "16x10.ini") == 0);
    CHECK(pc_layout_load_ratio("", "4x3") == 0 && pc_layout_count() == 0);
    CHECK(pc_layout_load_file("no/such/folder/16x9.ini") == 0); /* a missing file is the 4:3 base, not an error */

    /* the shipped files, when their folder is given: valid and (today) empty */
    if (argc > 1) {
        const char* ratios[2] = {"16x9", "16x10"};
        int r;
        for (r = 0; r < 2; r++) {
            char path[1024];
            FILE* f;
            snprintf(path, sizeof(path), "%slayout/%s.ini", argv[1], ratios[r]);
            f = fopen(path, "rb");
            CHECK(f != NULL);
            if (f != NULL) {
                fclose(f);
            }
            rows = pc_layout_load_ratio(argv[1], ratios[r]);
            printf("%s: %d rows, %d problems\n", path, rows, pc_layout_problems());
            CHECK(rows >= 0 && pc_layout_problems() == 0);
        }
    }

    pc_layout_clear();
    printf("pc_layout_test: %d of %d checks passed\n", s_checks - s_fails, s_checks);
    return s_fails == 0 ? 0 : 1;
}
