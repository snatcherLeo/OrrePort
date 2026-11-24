/* musyx_layout_check: one row per size or field offset, filled once from our MusyX headers and once from AxioDL's. */
#ifndef MUSYX_LAYOUT_H
#define MUSYX_LAYOUT_H
#include <stddef.h>
typedef struct LayoutRow {
    const char* what;
    size_t value;
} LayoutRow;
#define LAYOUT_ROWS 64
int layout_ours(LayoutRow* rows);
int layout_axio(LayoutRow* rows);
#define ROW_SIZE(T) rows[n].what = "sizeof(" #T ")", rows[n++].value = sizeof(T)
#define ROW_OFF(T, f) rows[n].what = #T "." #f, rows[n++].value = offsetof(T, f)
#endif
