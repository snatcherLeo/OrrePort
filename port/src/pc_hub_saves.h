/* pc_hub_saves.h: the hub's Saves page (pc_hub_saves.cpp says what it does and why). Called by pc_hub.cpp. */
#ifndef PC_HUB_SAVES_H
#define PC_HUB_SAVES_H

#include <aurora/input.hpp>
#include <RmlUi/Core.h>

#include <functional>
#include <string>

#include "pc_saves.h" /* PcSaveInfo (the region word's helpers) */

/* the hub's document, and what to call when the chosen save changes (the hub's card and PLAY) */
void hub_saves_attach(Rml::ElementDocument* doc, std::function<void()> changed);
void hub_saves_open();
void hub_saves_close();
bool hub_saves_is_open();
bool hub_saves_click(Rml::Element* target);                                        /* true when the page took it */
aurora::input::EventResult hub_saves_event(const aurora::input::InputEvent& event); /* while open */
void hub_saves_frame();                     /* each hub frame: files picked in the file window, the search box */
void hub_saves_name_press();                /* a mouse press (before its click): the Rename box's select-all */
/* a trainer name cut with "…" when wider than an ordinary long name ("Christopher"), measured in ref's font */
std::string hub_fit_name(Rml::Element* ref, const std::string& name);
/* a save name cut with "…" to room pixels in ref's font; wrap builds the printed text from it (2026-11-21); escaped */
std::string hub_fit_name_px(Rml::Element* ref, const std::string& name, int room,
                            const std::function<std::string(const std::string&)>& wrap);
/* a name in its drawn form (right-to-left names joined and reordered, pc_rtl.h; other names unchanged) */
std::string hub_shown_name(const std::string& typed);
/* The coloured region word of a save as RML (3.5 step 6; pc_hub_saves.cpp says why): "US" / "JPN" / "PAL · Spanish"
 * (full) or "PAL · ES" (short), in hub.rcss .rword's colours; id: the element id it gets, or nullptr; "" when the save has
 * no known region (an empty slot) */
std::string hub_region_rml(const PcSaveInfo& s, bool full, const char* id);
/* a save's line under its name as RML: the region word leading saves.row.region, or saves.row when there's none; the
 * trainer, time and date already as they should show (cut, formatted) */
std::string hub_save_line_rml(const PcSaveInfo& s, const std::string& trainer, const std::string& time,
                              const std::string& date, bool full, const char* id);
bool hub_saves_is_save_file(const char* path); /* .gci, .raw or .orresave */
void hub_saves_drop(const char* path);      /* a .gci or .raw dropped on the hub: imported (opens the page) */
void hub_saves_offer_copy(int slot);         /* accurate mode's PLAY on a save with long names: offer a GameCube copy */
void hub_saves_ask_loose();                  /* the import question for saves found loose in Saves\ at start-up (3.5) */
void hub_saves_test_import_ask(const char* slots); /* test hook (ORRE_HUB_TEST_SAVES=importask:<slot>,...): the question */
void hub_saves_test_versions();             /* test hook (ORRE_HUB_TEST_SAVES=versions): the page and its go-back window */
void hub_saves_test_accurate(int slot);     /* test hook (ORRE_HUB_TEST_SAVES=accurate:<slot>): that save's switch pressed */
void hub_saves_test_whatoff();              /* test hook (ORRE_HUB_TEST_SAVES=whatoff): the "What it turns off" list */
void hub_saves_test_gcopy(int slot, const char* path); /* test hook (gcopyacc:<slot>): the accurate copy, no Save As;
                                                         * path NULL (gcopy:<slot>, 2026-11-21): the copy window only */
void hub_saves_test_play_autosave(int slot); /* test hook (playauto:<slot>, 2026-11-21): "Play the autosave" pressed */
/* GameCube-accurate mode's "What it turns off" list as RML (pc_options.c's rows, in the player's language); the Saves
 * page and Settings › General both show it (2.12) */
std::string hub_accurate_list_rml();

#endif
