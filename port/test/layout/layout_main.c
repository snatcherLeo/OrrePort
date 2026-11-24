/*
 * musyx_layout_check: the structures our MusyX runtime and AxioDL's PC hardware layer both read must have the same
 * layout on the PC (docs/port/musyx-evaluation.md, "Step 2"). Prints each size / offset from both sides; exit 0 when
 * all agree.
 */
#include <stdio.h>
#include <string.h>
#include "layout.h"

int main(void) {
    LayoutRow ours[LAYOUT_ROWS], axio[LAYOUT_ROWS];
    int n = layout_ours(ours), m = layout_axio(axio), i, bad = 0;
    if (n != m) {
        printf("row count differs: %d vs %d\n", n, m);
        return 1;
    }
    for (i = 0; i < n; i++) {
        int same = ours[i].value == axio[i].value && strcmp(ours[i].what, axio[i].what) == 0;
        printf("%-34s ours %5zu  AxioDL %5zu  %s\n", ours[i].what, ours[i].value, axio[i].value, same ? "same" : "DIFFERENT");
        bad += !same;
    }
    printf("result: %s\n", bad ? "LAYOUTS DIFFER" : "ALL SAME");
    return bad ? 1 : 0;
}
