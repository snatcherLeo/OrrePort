/*
 * pc_settings.c: the port's settings file, <exe folder>/user/settings.ini (the portable folder, never AppData; snatcherLeo's
 * rule, design note 33). One "key = value" per line, '#' or ';' starts a comment; unknown keys are kept when the file
 * is written back, so a newer build's settings survive an older one. Session B, 2026-10-17 (window items 2 and 3).
 *
 * Today's keys: window_x, window_y, window_w, window_h (the window's last place and size, not while fullscreen),
 * fullscreen (0 / 1). Only the port reads it; nothing here reaches game code.
 * *Updated 2026-11-13:* window_maximized (0 / 1: maximized at the last close; the place and size above stay the
 * normal window's, for the restore button); window_x / window_y may be negative (a monitor left of or above the main
 * one). Many more keys since (the Settings rows); pc_settings_overlay.cpp lists them.
 */
#include "pc_settings.h"

#include <SDL3/SDL.h> /* SDL_GetTicks: the hub's time in the change lines */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* *Raised 2026-11-20 (found by the 3.7 research):* MAX_KEYS was 64, and a key past it was dropped without a word
 * on load and on save. The program has about 55 fixed keys plus per-controller, per-stick and per-option ones, and
 * snatcherLeo's own file already held 46, so settings were about to start vanishing. 512 keys cost about 130 KB of static
 * memory; reaching the limit is now logged (warn_full). Every OS the same. */
enum { MAX_KEYS = 512, KEY_LEN = 48, VALUE_LEN = 208 };

static char s_path[1200];
static char s_keys[MAX_KEYS][KEY_LEN];
static char s_values[MAX_KEYS][VALUE_LEN];
static unsigned char s_mine[MAX_KEYS]; /* set by this program since the last save (take_others_changes keeps these) */
static int s_count;
static int s_loaded;

/* a key that doesn't fit is said once in the log, never dropped silently */
static void warn_full(const char* key) {
    static int warned;
    if (!warned) {
        warned = 1;
        printf("[settings] settings.ini is full (%d keys): \"%s\" and any later new key are not kept\n", MAX_KEYS, key);
    }
}

static char* trim(char* s) {
    char* end;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }
    return s;
}

/* *Added 2026-11-22 (snatcherLeo: "we don't want that texture pack thing in settings.ini at all"):* keys that are no longer
 * settings. Unknown keys are kept on purpose (a newer build's settings survive an older one), so a key OrrePort retires
 * would otherwise stay in the player's file for good; a retired key is dropped as the file loads, and the next save
 * writes the file without it. Only keys we removed on purpose go here, never a newer build's. Every OS the same.
 * - texture_pack: the texture-pack switch, removed 2026-11-22 (packs return through the Mods folder and Studio) */
static int retired_key(const char* key) {
    static const char* const retired[] = {"texture_pack"};
    for (size_t i = 0; i < sizeof(retired) / sizeof(retired[0]); i++) {
        if (strcmp(key, retired[i]) == 0) return 1;
    }
    return 0;
}

void pc_settings_load(const char* user_folder) {
    char line[KEY_LEN + VALUE_LEN + 8];
    FILE* f;

    snprintf(s_path, sizeof(s_path), "%ssettings.ini", user_folder);
    s_count = 0;
    s_loaded = 1;
    memset(s_mine, 0, sizeof(s_mine));
    f = fopen(s_path, "r");
    if (f == NULL) {
        return; /* first run: defaults */
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        if (s_count >= MAX_KEYS) {
            warn_full("(the rest of the file)");
            break;
        }
        char* eq;
        char* key;
        char* value = trim(line);
        if (*value == '#' || *value == ';' || *value == '\0' || (eq = strchr(value, '=')) == NULL) {
            continue;
        }
        *eq = '\0';
        key = trim(value);
        value = trim(eq + 1);
        if (retired_key(key)) {
            printf("[settings] dropped the retired key \"%s\" (no longer a setting; gone from the file at the next save)\n",
                   key);
            continue;
        }
        snprintf(s_keys[s_count], KEY_LEN, "%s", key);
        snprintf(s_values[s_count], VALUE_LEN, "%s", value);
        s_count++;
    }
    fclose(f);
}

static int find(const char* key) {
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_keys[i], key) == 0) {
            return i;
        }
    }
    return -1;
}

int pc_settings_int(const char* key, int fallback) {
    int i = find(key);
    char* end;
    long v;
    if (i < 0) {
        return fallback;
    }
    v = strtol(s_values[i], &end, 0);
    return end == s_values[i] ? fallback : (int)v;
}

const char* pc_settings_string(const char* key, const char* fallback) {
    int i = find(key);
    return i < 0 || s_values[i][0] == '\0' ? fallback : s_values[i];
}

/* *Added 2026-11-20 (step 3.7, the logs carry every setting; docs/port/tweaks-37-research.md section 8):* every change
 * of a setting is one log line, "[settings] <key>: <old> -> <new> (game frame N)" (or "(hub +Ns)" before the game
 * runs), so a tester's log says what was switched and when. Why here: nearly every Settings row, shortcut and choice
 * goes through these two setters, so one place catches them all and a new setting is logged with no extra work. Keys
 * that change on their own all the time are left out (the window's place and size as it is dragged, the sticks'
 * calibration data, bookkeeping marks): their values are in the start-up snapshot (pc_settings_log.cpp). Every OS the
 * same (SDL's tick count for the hub's seconds). */
/* the game frame (pc_vi_heartbeat, 0 until the game runs), given by the game program at start-up
 * (pc_settings_set_frame_source): the settings file is also used by programs without the game, which have no frames */
static unsigned (*s_frame)(void);
void pc_settings_set_frame_source(unsigned (*frame)(void)) { s_frame = frame; }

static int noisy_key(const char* key) {
    static const char* const prefixes[] = {"window_", "debug_panel_", "ff_checked"};
    size_t n = strlen(key);
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        if (strncmp(key, prefixes[i], strlen(prefixes[i])) == 0) return 1;
    }
    /* the sticks' calibration: stick0_cal, stick1_cx, stick1_cy (not their range, dead zone or snap) */
    return strncmp(key, "stick", 5) == 0 &&
           ((n > 4 && strcmp(key + n - 4, "_cal") == 0) || (n > 3 && (strcmp(key + n - 3, "_cx") == 0 ||
                                                                       strcmp(key + n - 3, "_cy") == 0)));
}

void pc_settings_when(char* out, size_t cap) {
    const unsigned frame = s_frame != NULL ? s_frame() : 0;
    if (frame > 0) {
        snprintf(out, cap, "game frame %u", frame);
    } else {
        snprintf(out, cap, "hub +%.1f s", (double)SDL_GetTicks() / 1000.0);
    }
}

static void log_change(int i, const char* key, const char* value) {
    char when[48];
    if (noisy_key(key) || (i >= 0 && strcmp(s_values[i], value) == 0)) return;
    pc_settings_when(when, sizeof(when));
    printf("[settings] %s: %s -> %s (%s)\n", key, i >= 0 ? s_values[i] : "(not set)", value, when);
}

void pc_settings_set_int(const char* key, int value) {
    int i = find(key);
    char text[24];
    snprintf(text, sizeof(text), "%d", value);
    log_change(i, key, text);
    if (i < 0) {
        if (s_count >= MAX_KEYS) {
            warn_full(key);
            return;
        }
        i = s_count++;
        snprintf(s_keys[i], KEY_LEN, "%s", key);
    }
    snprintf(s_values[i], VALUE_LEN, "%d", value);
    s_mine[i] = 1;
}

void pc_settings_set_string(const char* key, const char* value) {
    int i = find(key);
    log_change(i, key, value);
    if (i < 0) {
        if (s_count >= MAX_KEYS) {
            warn_full(key);
            return;
        }
        i = s_count++;
        snprintf(s_keys[i], KEY_LEN, "%s", key);
    }
    snprintf(s_values[i], VALUE_LEN, "%s", value);
    s_mine[i] = 1;
}

/* every key in settings.ini as it is now (the start-up snapshot, pc_settings_log.cpp) */
int pc_settings_count(void) { return s_count; }
const char* pc_settings_key_at(int i) { return i >= 0 && i < s_count ? s_keys[i] : ""; }
const char* pc_settings_value_at(int i) { return i >= 0 && i < s_count ? s_values[i] : ""; }

/* *Added 2026-11-23 (Studio's finding, the update switches):* settings.ini has two writers now: OrrePort, and Studio,
 * which sets update_check and update_auto_download in it (and reads the theme and language). OrrePort kept the whole
 * file in memory from its start, so its next save wrote back the values it loaded, undoing a change Studio made in
 * between. So before writing, the file is read again, and every key this program didn't set since it loaded takes the
 * file's current value (a key another program added comes along too); the keys this program did set win. A setting
 * the other program changes is then also what this program reads from then on. Plain C files: every OS the same. */
static void take_others_changes(void) {
    char line[KEY_LEN + VALUE_LEN + 8];
    FILE* f = fopen(s_path, "r");
    if (f == NULL) return;
    while (fgets(line, sizeof(line), f) != NULL) {
        char* eq;
        char* key;
        char* value = trim(line);
        int i;
        if (*value == '#' || *value == ';' || *value == '\0' || (eq = strchr(value, '=')) == NULL) continue;
        *eq = '\0';
        key = trim(value);
        value = trim(eq + 1);
        if (retired_key(key)) continue;
        i = find(key);
        if (i >= 0 && s_mine[i]) continue; /* this program's own change wins */
        if (i < 0) {
            if (s_count >= MAX_KEYS) break;
            i = s_count++;
            snprintf(s_keys[i], KEY_LEN, "%s", key);
            s_mine[i] = 0;
        }
        if (strcmp(s_values[i], value) != 0) {
            printf("[settings] %s: %s -> %s (changed by another program, kept)\n", key, s_values[i], value);
            snprintf(s_values[i], VALUE_LEN, "%s", value);
        }
    }
    fclose(f);
}

/* Written to a temporary file and renamed, so a crash mid-write never leaves a half file */
void pc_settings_save(void) {
    char tmp[1300];
    FILE* f;

    if (!s_loaded || s_path[0] == '\0') {
        return;
    }
    take_others_changes();
    snprintf(tmp, sizeof(tmp), "%s.tmp", s_path);
    f = fopen(tmp, "w");
    if (f == NULL) {
        return;
    }
    fprintf(f, "# OrrePort settings (written by the program; edit while it is closed)\n");
    for (int i = 0; i < s_count; i++) {
        fprintf(f, "%s = %s\n", s_keys[i], s_values[i]);
    }
    fclose(f);
    remove(s_path);
    /* *Added 2026-11-23 (Studio's question: a switch flipped in Studio after OrrePort had flipped the same one):* once
     * written, this program's changes are in the file, so they stop being "mine": a later change by Studio is then
     * taken at the next save, unless this program sets that key again in between (then the newer change, this
     * program's, wins). Before, a key once set here stayed "mine" for the whole run and always overwrote Studio's. */
    if (rename(tmp, s_path) == 0) memset(s_mine, 0, sizeof(s_mine));
}
