/*
 * pc_layout.c: the UI-per-ratio layout table's reader (pc_layout.h says why; docs/port/widescreen-plan.md section 6).
 *
 * The file format (the header comment of port/ui/layout/16x9.ini says the same for whoever edits a file):
 *   ; a comment, on its own line (';' or '#')
 *   [window.0013]                 one section per piece; the id is the piece's stable id
 *   anchor = right                left | centre (center) | right | stretch | keep43
 *   offset = -12, 0               x, y in logical 640 x 480 pixels, added after the anchor
 *   span = full                   effects only: 43 | full
 *   border = black                kept panels only: a colour name, #rrggbb, or none
 *   note = why this row exists    free text for the UI Editor
 * Ids hold letters, digits, '.', '_' and '-' (read in lower case). Values run to the end of the line (a ';' inside a
 * note is part of the note). A section seen again replaces the earlier row whole.
 *
 * Why a hash map: the offset hooks will ask once per window and per drawn piece every frame (hundreds a frame in the
 * menus), so a lookup must not walk the rows. FNV-1a over the id, open addressing, table at most half full.
 * Platforms: plain C (stdio, stdlib), every OS.
 */
#include "pc_layout.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PcLayoutRow* s_rows;
static int s_count, s_capacity;
static int* s_table; /* indices into s_rows, -1 empty; size s_tableSize (a power of two) */
static int s_tableSize;
static int s_problems;

static unsigned hash_id(const char* id) {
    unsigned h = 2166136261u;
    for (; *id != '\0'; ++id) {
        h = (h ^ (unsigned char)*id) * 16777619u;
    }
    return h;
}

void pc_layout_clear(void) {
    free(s_rows);
    free(s_table);
    s_rows = NULL;
    s_table = NULL;
    s_count = s_capacity = s_tableSize = 0;
    s_problems = 0;
}

int pc_layout_count(void) { return s_count; }
int pc_layout_problems(void) { return s_problems; }

static void problem(const char* source, int line, const char* fmt, ...) {
    va_list args;
    ++s_problems;
    fprintf(stderr, "[layout] %s:%d: ", source != NULL ? source : "?", line);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

/* the slot holding id, or the empty slot where it would go */
static int table_slot(const char* id) {
    const unsigned mask = (unsigned)s_tableSize - 1u;
    unsigned i = hash_id(id) & mask;
    while (s_table[i] >= 0 && strcmp(s_rows[s_table[i]].id, id) != 0) {
        i = (i + 1u) & mask;
    }
    return (int)i;
}

static int table_grow(void) {
    const int size = s_tableSize == 0 ? 64 : s_tableSize * 2;
    int* table = (int*)malloc((size_t)size * sizeof(int));
    int i;
    if (table == NULL) {
        return 0;
    }
    free(s_table);
    s_table = table;
    s_tableSize = size;
    for (i = 0; i < size; i++) {
        s_table[i] = -1;
    }
    for (i = 0; i < s_count; i++) {
        s_table[table_slot(s_rows[i].id)] = i;
    }
    return 1;
}

/* add the row, or replace the row with its id */
static int put_row(const PcLayoutRow* row) {
    int slot;
    if (s_tableSize == 0 || (s_count + 1) * 2 > s_tableSize) {
        if (!table_grow()) {
            return 0;
        }
    }
    slot = table_slot(row->id);
    if (s_table[slot] >= 0) {
        s_rows[s_table[slot]] = *row;
        return 1;
    }
    if (s_count == s_capacity) {
        const int capacity = s_capacity == 0 ? 32 : s_capacity * 2;
        PcLayoutRow* rows = (PcLayoutRow*)realloc(s_rows, (size_t)capacity * sizeof(PcLayoutRow));
        if (rows == NULL) {
            return 0;
        }
        s_rows = rows;
        s_capacity = capacity;
    }
    s_rows[s_count] = *row;
    s_table[slot] = s_count++;
    return 1;
}

/* lower-cased copy of id into out; 0 if it is empty, too long or holds another character */
static int normalise_id(const char* id, size_t length, char out[PC_LAYOUT_ID_MAX]) {
    size_t i;
    if (length == 0 || length >= PC_LAYOUT_ID_MAX) {
        return 0;
    }
    for (i = 0; i < length; i++) {
        const unsigned char c = (unsigned char)id[i];
        if (!(isalnum(c) || c == '.' || c == '_' || c == '-')) {
            return 0;
        }
        out[i] = (char)tolower(c);
    }
    out[length] = '\0';
    return 1;
}

static void default_row(PcLayoutRow* row, const char* id) {
    memset(row, 0, sizeof(*row));
    if (id != NULL) {
        snprintf(row->id, sizeof(row->id), "%s", id);
    }
    row->anchor = PC_LAYOUT_CENTRE;
    row->span = PC_LAYOUT_SPAN_DEFAULT;
}

static int word_is(const char* value, const char* word) {
    while (*value != '\0' && *word != '\0' && tolower((unsigned char)*value) == *word) {
        ++value, ++word;
    }
    return *value == '\0' && *word == '\0';
}

static void copy_text(char* out, size_t size, const char* value) {
    snprintf(out, size, "%s", value);
}

/* one "key = value" line into the row being read */
static void read_key(PcLayoutRow* row, const char* key, const char* value, const char* source, int line) {
    if (word_is(key, "anchor")) {
        if (word_is(value, "left")) row->anchor = PC_LAYOUT_LEFT;
        else if (word_is(value, "centre") || word_is(value, "center")) row->anchor = PC_LAYOUT_CENTRE;
        else if (word_is(value, "right")) row->anchor = PC_LAYOUT_RIGHT;
        else if (word_is(value, "stretch")) row->anchor = PC_LAYOUT_STRETCH;
        else if (word_is(value, "keep43")) row->anchor = PC_LAYOUT_KEEP43;
        else problem(source, line, "[%s] anchor \"%s\": not left, centre, right, stretch or keep43 (kept: %s)", row->id,
                     value, pc_layout_anchor_name(row->anchor));
    } else if (word_is(key, "offset")) {
        char* end;
        const double x = strtod(value, &end);
        double y;
        char* end2;
        if (end == value) {
            problem(source, line, "[%s] offset \"%s\": not \"x, y\"", row->id, value);
            return;
        }
        while (*end == ' ' || *end == '\t' || *end == ',') {
            ++end;
        }
        y = strtod(end, &end2);
        if (end2 == end) {
            problem(source, line, "[%s] offset \"%s\": no y (read as x, 0)", row->id, value);
            y = 0.0;
        }
        row->offsetX = (float)x;
        row->offsetY = (float)y;
    } else if (word_is(key, "span")) {
        if (word_is(value, "43") || word_is(value, "4:3")) row->span = PC_LAYOUT_SPAN_43;
        else if (word_is(value, "full")) row->span = PC_LAYOUT_SPAN_FULL;
        else problem(source, line, "[%s] span \"%s\": not 43 or full", row->id, value);
    } else if (word_is(key, "border")) {
        if (strlen(value) >= PC_LAYOUT_BORDER_MAX) {
            problem(source, line, "[%s] border \"%s\": longer than %d characters (cut)", row->id, value,
                    PC_LAYOUT_BORDER_MAX - 1);
        }
        copy_text(row->border, sizeof(row->border), value);
    } else if (word_is(key, "note")) {
        copy_text(row->note, sizeof(row->note), value);
    } else {
        problem(source, line, "[%s] unknown key \"%s\" (left out)", row->id, key);
    }
}

int pc_layout_load_text(const char* text, size_t length, const char* sourceName) {
    const char* p = text;
    const char* const end = text + length;
    PcLayoutRow row;
    int inRow = 0, read = 0, line = 0;
    int skipping = 0; /* inside a section whose id was refused: its keys are left out quietly (one problem said so) */
    while (p < end) {
        const char* lineEnd = p;
        const char *a, *b;
        while (lineEnd < end && *lineEnd != '\n') {
            ++lineEnd;
        }
        ++line;
        a = p;
        b = lineEnd;
        p = lineEnd < end ? lineEnd + 1 : end;
        while (a < b && (*a == ' ' || *a == '\t' || *a == '\r')) ++a;
        while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r')) --b;
        if (line == 1 && b - a >= 3 && (unsigned char)a[0] == 0xEF && (unsigned char)a[1] == 0xBB &&
            (unsigned char)a[2] == 0xBF) {
            a += 3; /* a UTF-8 byte-order mark (Windows editors write one) */
        }
        if (a == b || *a == ';' || *a == '#') {
            continue;
        }
        if (*a == '[') {
            char id[PC_LAYOUT_ID_MAX];
            if (inRow) {
                if (!put_row(&row)) return -1;
                ++read;
            }
            inRow = 0;
            skipping = 0;
            if (b[-1] != ']' || !normalise_id(a + 1, (size_t)(b - a - 2), id)) {
                skipping = 1;
                problem(sourceName, line, "section \"%.*s\": not [id] with letters, digits, '.', '_' or '-' (up to %d); "
                        "its keys are left out", (int)(b - a), a, PC_LAYOUT_ID_MAX - 1);
                continue;
            }
            default_row(&row, id);
            row.fromFile = 1;
            inRow = 1;
            continue;
        }
        {
            const char* eq = memchr(a, '=', (size_t)(b - a));
            char key[32], value[PC_LAYOUT_NOTE_MAX + 64];
            const char *k1 = a, *k2, *v1, *v2 = b;
            if (eq == NULL) {
                problem(sourceName, line, "\"%.*s\": not \"key = value\" (left out)", (int)(b - a), a);
                continue;
            }
            k2 = eq;
            while (k2 > k1 && (k2[-1] == ' ' || k2[-1] == '\t')) --k2;
            v1 = eq + 1;
            while (v1 < v2 && (*v1 == ' ' || *v1 == '\t')) ++v1;
            if (skipping) {
                continue;
            }
            if (!inRow) {
                problem(sourceName, line, "\"%.*s\": outside any [section] (left out)", (int)(b - a), a);
                continue;
            }
            snprintf(key, sizeof(key), "%.*s", (int)(k2 - k1), k1);
            if ((size_t)(v2 - v1) >= sizeof(value)) {
                problem(sourceName, line, "[%s] %s: the value is too long (cut)", row.id, key);
            }
            snprintf(value, sizeof(value), "%.*s", (int)(v2 - v1), v1);
            read_key(&row, key, value, sourceName, line);
        }
    }
    if (inRow) {
        if (!put_row(&row)) return -1;
        ++read;
    }
    return read;
}

int pc_layout_load_file(const char* path) {
    FILE* f = fopen(path, "rb");
    char* text;
    long size;
    int rows;
    if (f == NULL) {
        return 0; /* no file: the 4:3 base */
    }
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    text = (char*)malloc((size_t)size + 1);
    if (text == NULL || fread(text, 1, (size_t)size, f) != (size_t)size) {
        free(text);
        fclose(f);
        return -1;
    }
    fclose(f);
    text[size] = '\0';
    rows = pc_layout_load_text(text, (size_t)size, path);
    free(text);
    return rows;
}

const char* pc_layout_file_for_ratio(const char* ratio) {
    if (ratio == NULL) return NULL;
    if (strcmp(ratio, "16x9") == 0) return "16x9.ini";
    if (strcmp(ratio, "16x10") == 0) return "16x10.ini";
    if (strcmp(ratio, "21x9") == 0) return "21x9.ini"; /* later (with Studio's UI Editor); read if it exists */
    return NULL;                                        /* 4x3 and anything else: the base, no file */
}

int pc_layout_load_ratio(const char* uiDir, const char* ratio) {
    const char* name = pc_layout_file_for_ratio(ratio);
    char path[1024];
    int rows;
    pc_layout_clear();
    if (name == NULL) {
        fprintf(stderr, "[layout] %s: no file (the 4:3 base: every piece at its 4:3 place)\n",
                ratio != NULL ? ratio : "(none)");
        return 0;
    }
    snprintf(path, sizeof(path), "%slayout/%s", uiDir != NULL ? uiDir : "", name);
    rows = pc_layout_load_file(path);
    fprintf(stderr, "[layout] %s: %s, %d row%s (%d problem%s); a piece with no row keeps its 4:3 place, centred\n",
            ratio, path, rows, rows == 1 ? "" : "s", s_problems, s_problems == 1 ? "" : "s");
    return rows;
}

const PcLayoutRow* pc_layout_find(const char* id) {
    char key[PC_LAYOUT_ID_MAX];
    int slot;
    if (s_count == 0 || id == NULL || !normalise_id(id, strlen(id), key)) {
        return NULL;
    }
    slot = table_slot(key);
    return s_table[slot] >= 0 ? &s_rows[s_table[slot]] : NULL;
}

PcLayoutRow pc_layout_lookup(const char* id) {
    const PcLayoutRow* row = pc_layout_find(id);
    PcLayoutRow answer;
    if (row != NULL) {
        return *row;
    }
    default_row(&answer, id);
    return answer;
}

const char* pc_layout_anchor_name(PcLayoutAnchor anchor) {
    switch (anchor) {
    case PC_LAYOUT_LEFT: return "left";
    case PC_LAYOUT_RIGHT: return "right";
    case PC_LAYOUT_STRETCH: return "stretch";
    case PC_LAYOUT_KEEP43: return "keep43";
    default: return "centre";
    }
}

const char* pc_layout_span_name(PcLayoutSpan span) {
    switch (span) {
    case PC_LAYOUT_SPAN_43: return "43";
    case PC_LAYOUT_SPAN_FULL: return "full";
    default: return "default";
    }
}
