/* pc_hub_langdiscs.h: the hub's Language discs link and panel (pc_hub_langdiscs.cpp says what it does and why;
 * docs/port/language-discs.md). Called by pc_hub.cpp and the Settings panel. */
#ifndef PC_HUB_LANGDISCS_H
#define PC_HUB_LANGDISCS_H

#include <aurora/input.hpp>
#include <RmlUi/Core.h>

void hub_lang_attach(Rml::ElementDocument* doc); /* once the hub's page is loaded */
void hub_lang_open(bool open);                   /* unfold / fold the panel (Settings › General opens it unfolded) */
bool hub_lang_is_open();
bool hub_lang_click(Rml::Element* target);       /* true when the link or the panel took the click */
bool hub_lang_event(const aurora::input::InputEvent& event); /* the controller and arrow keys; true when taken */
void hub_lang_frame();                           /* each hub frame: picked files, the copy, the folders' watch */
bool hub_lang_drop(const char* path);            /* a European or Japanese disc dropped on the hub: offered here */
/* the controller's place here (2026-11-15, the hub's full navigation in pc_hub.cpp): -1 nothing, 0 the link, 1... the
 * panel's buttons; set clamps to what is shown */
int hub_lang_focus();
void hub_lang_set_focus(int f);
int hub_lang_buttons(); /* how many places: the link and the panel's buttons */
/* the Japanese release look's "What changes" window (3.5 step 5): open, and its input while open (it takes all) */
bool hub_lang_look_is_open();
aurora::input::EventResult hub_lang_look_event(const aurora::input::InputEvent& event);

#endif
