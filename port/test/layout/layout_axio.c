/* AxioDL MusyX's types (extern/musyx-pc/include), as its PC hardware layer sees them. */
#include "musyx/musyx.h"
#include "musyx/hardware.h"
#include "musyx/voice.h"
#include "layout.h"

int layout_axio(LayoutRow* rows) {
    int n = 0;
#include "layout_rows.h"
    return n;
}
