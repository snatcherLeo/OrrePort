/* Our MusyX runtime's types (include/musyx/runtime). */
#include "dolphin/types.h"
#include "musyx/runtime/hooks_types.h"
#include "musyx/runtime/vsamples_types.h"
#include "musyx/runtime/reverb.h"
#include "layout.h"

int layout_ours(LayoutRow* rows) {
    int n = 0;
#include "layout_rows.h"
    return n;
}
