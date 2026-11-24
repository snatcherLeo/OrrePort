/*
 * pc_text.h: every word OrrePort shows, read from a language file, so anyone can translate it (snatcherLeo, 2026-11-13: "We
 * want that design phiosophy as well for OrrePort Studio, because, then anyone from any region can use and enjoy
 * creating and playing with these games"; "whatefver way we can make it, so that, when I gain collaborators on discord
 * to translate, it'll be very easy"). The rule: no text the player sees without a key.
 *
 * Where the words live: <exe>/ui/lang/<code>.txt, one file per language, plain UTF-8 text, "key = text" lines and '#'
 * comments (the same shape as Java .properties, which translation sites such as Crowdin and Weblate read). en.txt is
 * the source and the fallback: a key a translation lacks shows the English. A translator copies en.txt to e.g. de.txt,
 * changes the text after each '=', and drops it into ui/lang/; no rebuild. tools/local/lang_check.py lists what is
 * missing, extra or broken. Which language: settings.ini "language" (default en), or ORRE_LANG for a test run;
 * "pseudo" is a made-up test language (every text accented and about 40% longer) that shows anything not yet on keys
 * and anything that can't grow.
 *
 * In a text:
 *   {name}   a value filled in by OrrePort (a file name, a number); a translator may move it, never rename it.
 *   \n       a new line.
 * Counts: a key with forms, e.g. "lshelf.count.one = {n} picture" and "lshelf.count.other = {n} pictures"; the
 * language file's "plural" line says which forms the language has (see en.txt). Dates: "date.time" with {day}
 * {month} {year} {hh} {mm}, and the month names "month.1" ... "month.12".
 *
 * Pages (RmlUi .rml) write "[key]" where the text goes; Aurora's page reader hands every text to pc_text (the RmlUi
 * hook, rmlui.hpp set_text_translator). C and C++ code calls pc_text / pc_text_format.
 *
 * Platforms: plain C and SDL file reading; the same on Windows, Linux and macOS (verified on Windows; likely elsewhere).
 */
#ifndef PC_TEXT_H
#define PC_TEXT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* reads ui/lang/en.txt and the chosen language's file; safe to call again (reloads) */
void pc_text_init(void);
/* the text for a key in the player's language (English if the language lacks it; the key itself if English lacks it
 * too, so a missing key shows on screen and in the log). The pointer stays valid until the next pc_text_init. */
const char* pc_text(const char* key);
/* the text with its {name} placeholders filled: name / value pairs after the key, ending with NULL. Returns out. */
const char* pc_text_format(char* out, size_t size, const char* key, ...);
/* the language in use ("en", "de", "pseudo") */
const char* pc_text_language(void);
/* the text for a key in a given language (OrrePort's words inside the game's screens follow the game's language,
 * pc_game_text_language); English when that language's file lacks it. Valid for the whole run. */
const char* pc_text_lang(const char* code, const char* key);

#ifdef __cplusplus
}

#include <initializer_list>
#include <string>
#include <utility>

/* C++: tx("hub.note.xd") or tx("hub.note.copy_failed", {{"error", why}}) */
std::string tx(const char* key, std::initializer_list<std::pair<const char*, std::string>> values = {});
/* the same in a given language (pc_text_lang): OrrePort's words inside the game's screens, by the game's language */
std::string tx_lang(const char* code, const char* key,
                    std::initializer_list<std::pair<const char*, std::string>> values = {});
/* a count: picks the key's form for n in this language (key.one, key.few, key.many, key.other) and fills {n} */
std::string tx_count(const char* key, long n, std::initializer_list<std::pair<const char*, std::string>> values = {});
/* a date and time in this language's pattern ("date.time", or "date.time.long" with the month written out), the time
 * 12- or 24-hour by the computer's own clock setting: "Nov 15, 2026, 6:19 AM" / "November 15, 2026, 6:19 AM" */
std::string tx_date(int year, int month, int day, int hour, int minute, bool longMonth = false);
/* the time of day alone, 12- or 24-hour by the computer's clock setting ("6:19 AM" / "06:19") */
std::string tx_time(int hour, int minute);
/* RmlUi pages: hook the page reader (call once, before any page loads; the RmlUi build only) */
void pc_text_hook_pages(void);
#include <vector>
/* the languages there are: (code, the name it gives itself in "language.name"), English first, then by name */
std::vector<std::pair<std::string, std::string>> pc_text_languages();
#endif

#endif
