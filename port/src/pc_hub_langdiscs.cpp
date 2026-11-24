/*
 * pc_hub_langdiscs.cpp: the hub's Language discs (region work piece 2, 2026-11-15; docs/port/language-discs.md).
 *
 * What it does: one more link in the row under the save card ("Add a language disc…", then "Language discs" once one
 * is added) that unfolds a panel listing the player's European and Japanese discs (in Colosseum\ROM Other Regions\),
 * the languages each adds to the game's Options row, Remove, and Add for a disc still missing. Adding works every way:
 * the panel's file window, a disc dropped on the hub, or a file put in ROM Other Regions\ by hand (noticed within two
 * seconds); a European or Japanese disc put into ROM\ by hand is moved over, since ROM\ holds only the disc that's
 * played. After any change the European text is read again (pc_region_text_rescan), so the languages are in the
 * game's Options when PLAY is pressed.
 *
 * Why this way (snatcherLeo's design, 2026-11-15: "It preserves our minimal UI, for most users who wouldn't import the other
 * languages, but for those that do, they click and then see that window. They can toggle that window off by clicking
 * the tagline again"): the panel is folded at every start; most players never see more than the link. It lists the
 * discs of the game picked under the disc (Colosseum now; XD's own later, docs/port/language-discs.md), at most two.
 *
 * Controller: Down on the main page selects the link (Up goes back to PLAY), A opens or closes the panel, Down / Up walk
 * its buttons, A presses one, B folds the panel. The mouse clicks as anywhere in the hub.
 *
 * Nothing is deleted outright: Remove asks, then sends the file to the Recycle Bin (pc_recycle_file); a replaced disc
 * goes there too, once its replacement is in place. Copies are written as "<name>.part" and renamed when whole.
 *
 * Platforms: RmlUi, SDL file calls and threads, nod (pc_region_text.cpp): the same on Windows, Linux and macOS
 * (verified on Windows; likely elsewhere); the Recycle Bin is Windows' (pc_saves.c says what the others do).
 *
 * *Changed 2026-11-23 (snatcherLeo chose plan A, "one Discs panel"; Studio agreed, its Settings › Discs already had this
 * shape):* the panel is "Discs": the US disc on top (Replace… and Remove), then the European and Japanese rows, each
 * shown whether added or not ("Not added" and Add…, or Replace… and Remove), in the same order and with the same words
 * as Studio's (the shared discs.* keys in ui/lang). A disc picked from any row goes to its own row by its game ID.
 * The US disc is the one OrrePort has open, and Windows won't let an open file be replaced or removed (verified
 * 2026-11-23), so its Replace and Remove end in a restart: the new disc is copied in first (as "<name>.part", renamed
 * when whole, stamped "now" so the shared newest-original rule picks it), then OrrePort closes the disc, sends the old
 * file to the Recycle Bin and exits with the update code, which its launcher (OrrePort.exe) answers by starting it
 * again. Why a restart and not a reopen in place: the start-up checks (the whole-disc check above all) run on the new
 * disc as on any start, and nothing that read the old disc at start (the text, the fonts, the region tables) has to
 * be found and read again. Started without the launcher (a development run) OrrePort just closes. The restart is
 * Windows' launcher today; Linux and macOS would restart from their launch scripts the same way (likely).
 */
#include "pc_hub_langdiscs.h"

#include <SDL3/SDL.h>
#include <aurora/rmlui.hpp> /* process_input: the mouse in the "What changes" window */

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "pc_region_look.h" /* the Japanese release look's table: the "What changes" list (3.5 step 5) */
#include "pc_region_text.h"
#include "pc_saves.h"
#include "pc_text.h"
#include "pc_settings.h" /* pc_settings_save: before the US disc's restart */
#include "pc_update.h"   /* PC_UPDATE_EXIT_RESTART, pc_update_shutdown: the launcher's "start again" code */
#include "pc_zip.h" /* pc_file_touch_utf8: a moved disc counts as the newest (4.4) */

extern "C" const char* pc_platform_rom_folder(char* out, size_t size); /* pc_platform.c: <exe>/Colosseum/ROM/ */
extern "C" const char* pc_platform_disc_image(void);                   /* pc_platform.c: the US disc OrrePort opened */
extern "C" void pc_log_exit_clean(const char* how);                    /* pc_log.c: clean-exit line + drain */
#include <aurora/dvd.h> /* aurora_dvd_close: lets go of the open US disc before it is replaced or removed */

namespace {

Rml::ElementDocument* s_doc = nullptr;
bool s_open = false;     /* the panel unfolded */
bool s_look_open = false; /* the Japanese release look's "What changes" window is open (3.5 step 5) */
int s_focus = -1;        /* the controller's place: -1 none (PLAY), 0 the link, 1.. the panel's buttons in order */
std::vector<std::string> s_buttons; /* the focusable ids in order: "b-lang", then the panel's buttons */
std::vector<PcLangDisc> s_discs;
std::string s_note;      /* the panel's last line: what just happened */
bool s_note_ok = true;
/* (UI ids kept from the Language discs panel, so the hub's code and styles need no change: the link "b-lang", the
 * panel "langpanel", buttons "lp-*"; a row's region letter ends its ids: E the US disc, P European, J Japanese) */

/* what the panel is asking, if anything */
enum Ask { ASK_NONE, ASK_REMOVE, ASK_OFFER, ASK_REPLACE };
Ask s_ask = ASK_NONE;
char s_ask_region = 0;    /* 'E' (the US disc), 'P' or 'J' */
std::string s_ask_path;   /* the offered file */
uint64_t s_ask_size = 0;

/* a copy or move in progress (its own thread) */
std::atomic<int> s_copy_state{0}; /* 0 none, 1 running, 2 done, 3 failed */
std::atomic<uint64_t> s_copy_done{0}, s_copy_total{0};
std::string s_copy_src, s_copy_dst, s_copy_error, s_copy_replace; /* replace: the old file, recycled once done */
bool s_copy_move = false;
char s_copy_region = 0;

std::mutex s_pick_mutex;
std::string s_picked; /* from the file window (its callback may run on another thread) */

std::string esc(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    return out;
}

std::string folder() {
    char buf[1300];
    return pc_lang_discs_folder(buf, sizeof(buf));
}
std::string rom_folder() {
    char buf[1300];
    return pc_platform_rom_folder(buf, sizeof(buf));
}
std::string file_name(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
std::string gigabytes(uint64_t bytes) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", bytes / 1073741824.0);
    return tx("hub.add.size", {{"gb", buf}});
}
const char* disc_key(char region) { return region == 'E' ? "discs.us" : region == 'J' ? "discs.jp" : "discs.eu"; }
const PcLangDisc* disc_of(char region) {
    for (const PcLangDisc& d : s_discs)
        if (d.id[3] == region) return &d;
    return nullptr;
}
/* the US disc OrrePort opened ("" in the no-disc hub, which has its own ways to add one) */
std::string us_disc() { return pc_platform_disc_image(); }
/* two paths naming the same file (Windows: case and slash direction don't matter) */
bool same_path(std::string a, std::string b) {
    for (std::string* s : {&a, &b})
        for (char& c : *s) c = c == '\\' ? '/' : (char)std::tolower((unsigned char)c);
    return a == b;
}
/* a name in dir that no file has yet: "<name>", else "<name> (2)<ext>", "(3)", ... (a US disc copied in beside the
 * open one with the same file name: the open file can't be overwritten) */
std::string free_name(const std::string& dir, const std::string& name) {
    if (!SDL_GetPathInfo((dir + name).c_str(), nullptr)) return dir + name;
    const size_t dot = name.find_last_of('.');
    const std::string stem = dot == std::string::npos ? name : name.substr(0, dot);
    const std::string ext = dot == std::string::npos ? "" : name.substr(dot);
    for (int i = 2; i < 100; ++i) {
        const std::string p = dir + stem + " (" + std::to_string(i) + ")" + ext;
        if (!SDL_GetPathInfo(p.c_str(), nullptr)) return p;
    }
    return dir + name;
}

/* The end of the US disc's Replace or Remove: OrrePort lets go of the disc, the old file goes to the Recycle Bin, and
 * OrrePort exits with the update code, which its launcher answers by starting it again (the file's top comment says
 * why). Remove: 0 when the file couldn't be recycled (then the disc is opened again and nothing else changes). Replace:
 * the restart happens either way, since the new disc is already in and stamped newest, so the shared rule picks it;
 * an old file that couldn't be recycled just stays in ROM\ (logged). */
int us_disc_finish(const std::string& old, const char* why, bool remove) {
    std::printf("[langdiscs] US disc: %s; closing it to recycle %s\n", why, old.c_str());
    aurora_dvd_close();
    if (!old.empty() && !pc_recycle_file(old.c_str())) {
        if (remove) {
            std::printf("[langdiscs] US disc: %s couldn't go to the Recycle Bin; opened again\n", old.c_str());
            aurora_dvd_open(old.c_str());
            return 0;
        }
        std::printf("[langdiscs] US disc: %s couldn't go to the Recycle Bin; it stays, the newer disc is used\n",
                    old.c_str());
    }
    std::printf("[langdiscs] US disc: restarting (exit code %#x for the launcher)\n", (unsigned)PC_UPDATE_EXIT_RESTART);
    pc_settings_save();
    pc_update_shutdown();
    std::fflush(stdout);
    std::fflush(stderr);
    pc_log_exit_clean(remove ? "restarting after the US disc's Remove" : "restarting after the US disc's Replace");
    _Exit(PC_UPDATE_EXIT_RESTART);
}

void refresh_list() {
    PcLangDisc buf[4];
    const int n = pc_lang_discs_list("GC6", buf, 4);
    s_discs.assign(buf, buf + n);
}

void set_note(const std::string& text, bool ok) {
    s_note = text;
    s_note_ok = ok;
}

/* The Japanese release look in the panel (3.5 step 5; the project plan's "Showing players what changes", snatcherLeo approved
 * 2026-11-17): a line that depends on the Japanese disc, ending in a "What changes ›" link that opens a window with one
 * line per row of the region-differences table (ui/regions/jp_look.txt through pc_region_look.h; each line's words are
 * the row's own "what" key, hub.lang.jp_look.<entry>). Why from the table: the swap code, this list and later Studio
 * read the same rows, so a row added there shows here without code. Why a window and not a list unfolded in the panel:
 * the first picture (2026-11-19) had the eleven lines running under the hub's bottom bar at a normal window size; the
 * window is the pattern "What it turns off" set (a link, then a scrolling list over the page), it keeps the panel as
 * small as snatcherLeo wanted it (2026-11-15), and it has room for the plan's later before / after pictures. Each line says
 * whether it applies now: a check mark when the Japanese disc is added and OrrePort makes that change (the table's
 * "how" is sprite or entry), dimmed without the disc, and "(in a later update)" for a change not made yet (how = none:
 * Rui's 3D models until 3.5 step 4). The look also needs the game's language set to Japanese, which the line says (the
 * hub doesn't know the in-game language choice). UI ids: hub.lang.jp_look (the line), hub.lang.jp_look.what (the
 * link), hub.lang.jp_look.window (the window); elements lp-jplook-sec, lp-jplook, lookbox, looklist, look-close, and
 * one "lpjl-<row id>" per line (the row's stable id, dots as dashes). All platforms (RmlUi) */
std::string look_rml() {
    const bool have = disc_of('J') != nullptr;
    s_buttons.push_back("lp-jplook");
    return "<div class='lplook' id='lp-jplook-sec'><span class='lplookline'>" +
           esc(tx(have ? "hub.lang.jp_look.line_have" : "hub.lang.jp_look.line_add")) +
           /* the arrow joined to the words by a no-break space: as its own span RmlUi wrapped it alone onto the next
            * line (seen in the light-mode picture, 2026-11-19) */
           "</span><span class='lplink' id='lp-jplook'>" + esc(tx("hub.lang.jp_look.what")) + "&nbsp;›</span></div>";
}

/* the window's list, drawn when it opens (the disc may have been added or removed since) */
void look_window(bool open) {
    s_look_open = open;
    if (s_doc == nullptr) return;
    if (Rml::Element* body = s_doc->GetElementById("body")) body->SetClass("lookopen", open);
    if (!open) return;
    const bool have = disc_of('J') != nullptr;
    const bool off = pc_region_look_off() != 0; /* ORRE_NO_REGION_LOOK: a test switch turns every swap off */
    if (Rml::Element* e = s_doc->GetElementById("lookline"))
        e->SetInnerRML(esc(tx(have ? "hub.lang.jp_look.line_have" : "hub.lang.jp_look.line_add")));
    std::string h;
    int shown = 0;
    /* *Changed 2026-11-21 (snatcherLeo: "we can just condense the Rui changes"):* rows that share a "what" key (Rui's five, Somek's
     * three) show their line once, at the first such row; the later ones are skipped. A shared line is marked "now" when
     * its first row applies (rows sharing a key are the same kind of swap, all on or all later) */
    std::vector<std::string> said;
    for (int i = 0; i < pc_region_look_count(); ++i) {
        const PcRegionLookRow* r = pc_region_look_row(i);
        if (r == nullptr || std::strcmp(r->region, "JP") != 0) continue;
        if (std::find(said.begin(), said.end(), std::string(r->what)) != said.end()) continue;
        said.emplace_back(r->what);
        const bool later = std::strcmp(r->how, "none") == 0;
        const bool now = have && !off && !later;
        std::string id = std::string("lpjl-") + r->id;
        std::replace(id.begin(), id.end(), '.', '-');
        h += std::string("<div class='lpli") + (now ? " on" : "") + "' id='" + id + "'><span class='lpmark'>" +
             (now ? "✓" : "•") + "</span><span class='lplit'>" + esc(tx(r->what));
        if (later) h += " <span class='lplater'>" + esc(tx("hub.lang.jp_look.later")) + "</span>";
        h += "</span></div>";
        ++shown;
    }
    if (Rml::Element* e = s_doc->GetElementById("looklist")) {
        e->SetInnerRML(h);
        e->SetScrollTop(0.f);
    }
    std::printf("[langdiscs] What changes: %d line(s), Japanese disc %s%s\n", shown, have ? "added" : "not added",
                off ? " (ORRE_NO_REGION_LOOK: none apply)" : "");
}

/* the link and the panel, drawn from the state above */
void render() {
    if (s_doc == nullptr) return;
    if (Rml::Element* link = s_doc->GetElementById("b-langtext")) {
        link->SetInnerRML(esc(tx("hub.discs.link")));
    }
    if (Rml::Element* body = s_doc->GetElementById("body")) body->SetClass("langopen", s_open);
    Rml::Element* panel = s_doc->GetElementById("langpanel");
    if (panel == nullptr) return;
    s_buttons.assign(1, "b-lang");
    if (!s_open) {
        panel->SetInnerRML("");
        if (Rml::Element* col = s_doc->GetElementById("right")) col->SetScrollTop(0.f); /* folded: the column's top */
        if (s_focus > 0) s_focus = 0;
        if (Rml::Element* link = s_doc->GetElementById("b-lang")) link->SetClass("kfocus", s_focus == 0);
        return;
    }
    auto button = [](const char* id, const std::string& label, bool primary = false) {
        s_buttons.push_back(id);
        /* not the hub's .btn (a large fixed button); the panel's own smaller one (hub.rcss .lpbtn) */
        return std::string("<span class='lpbtn") + (primary ? " lpgo" : "") + "' id='" + id + "'>" + esc(label) +
               "</span>";
    };
    auto link = [](const std::string& id, const std::string& label) {
        s_buttons.push_back(id);
        return "<span class='lplink' id='" + id + "'>" + esc(label) + "</span>";
    };
    /* what a row is asking or doing, drawn right under it (the question, the copy's progress) */
    auto under = [&](char r) {
        std::string u;
        if (s_copy_state == 1 && s_copy_region == r) {
            const uint64_t total = s_copy_total, done = s_copy_done;
            u = "<div class='lpask'><div class='lpasktext'>" +
                esc(tx(s_copy_move ? "discs.moving" : "discs.copying",
                       {{"disc", tx(disc_key(r))},
                        {"percent", std::to_string(total > 0 ? (int)(done * 100 / total) : 0)}})) +
                "</div></div>";
        } else if (s_ask == ASK_REMOVE && s_ask_region == r) {
            u = "<div class='lpask'><div class='lpasktext'>" +
                esc(r == 'E' ? tx("hub.discs.remove_ask_us") : tx("hub.lang.remove_ask", {{"disc", tx(disc_key(r))}})) +
                "</div><div class='lpbtns'>" + button("lp-yes", tx("discs.remove")) +
                button("lp-no", tx("common.cancel")) + "</div></div>";
        } else if ((s_ask == ASK_OFFER || s_ask == ASK_REPLACE) && s_ask_region == r) {
            const std::string disc = tx(disc_key(r)), file = file_name(s_ask_path);
            std::string text =
                r == 'E'              ? tx("hub.discs.replace_ask_us", {{"file", file}})
                : s_ask == ASK_REPLACE ? tx("hub.lang.replace_ask", {{"disc", disc}, {"file", file}})
                                       : tx("discs.found", {{"disc", disc}, {"file", file}, {"size", gigabytes(s_ask_size)}});
            text += " " + tx("discs.choice", {{"size", gigabytes(s_ask_size)}});
            u = "<div class='lpask'><div class='lpasktext'>" + esc(text) + "</div><div class='lpbtns'>" +
                button("lp-copy", tx("discs.copy"), true) + button("lp-move", tx("discs.move")) +
                button("lp-no", tx("common.cancel")) + "</div></div>";
        }
        return u;
    };
    const bool busy = s_copy_state == 1;
    std::string h = "<div class='lphead'>" + esc(tx("hub.discs.heading")) + "</div>";
    /* the US disc, on top (the one played; Replace and Remove end in a restart, the file's top comment) */
    const std::string us = us_disc();
    if (!us.empty()) {
        h += "<div class='lprow'><img class='lpsym' src='hubicon://langus'/><div class='lptext'><div class='lpname'>" +
             esc(tx("discs.us")) + " <span class='lpid'>GC6E01</span></div><div class='lpfile'>" +
             esc(tx("hub.discs.us_line", {{"file", file_name(us)}})) + "</div></div>";
        if (!busy && s_ask == ASK_NONE)
            h += "<div class='lpacts'>" + link("lp-replace-E", tx("discs.replace")) + link("lp-remove-E", tx("discs.remove")) +
                 "</div>";
        h += "</div>" + under('E');
    }
    h += "<div class='lpline'>" + esc(tx("hub.lang.line")) + "</div>";
    /* the European and Japanese rows, each shown whether added or not (as in Studio's Settings › Discs) */
    for (const char r : {'P', 'J'}) {
        const PcLangDisc* d = disc_of(r);
        h += std::string("<div class='lprow") + (d == nullptr ? " lpnone" : "") + "'><img class='lpsym' src='hubicon://lang" +
             (r == 'J' ? "jp" : "eu") + "'/>";
        h += "<div class='lptext'><div class='lpname'>" + esc(tx(disc_key(r))) + " <span class='lpid'>" +
             (d != nullptr ? esc(d->id) : esc(tx("discs.not_added"))) + "</span></div><div class='lpchips'>";
        if (r == 'J') {
            /* Japanese text from the disc since piece 3 (2026-11-15). *3.5 step 5 (2026-11-19):* hub.lang.jp_later is
             * retired for Colosseum (never shown here); the key stays in en.txt for XD's Japanese disc, likely needed
             * while OrrePort can't show XD's Japanese text (guess). The Japanese release look's line follows below */
            h += "<span class='lpchip'>" + esc(tx("hub.lang.name.ja")) + "</span>";
        } else {
            for (const char* code : {"de", "fr", "es", "it"})
                h += "<span class='lpchip'>" + esc(tx((std::string("hub.lang.name.") + code).c_str())) + "</span>";
        }
        h += "</div></div>";
        if (!busy && s_ask == ASK_NONE) {
            h += "<div class='lpacts'>" +
                 (d != nullptr ? link(std::string("lp-replace-") + r, tx("discs.replace")) +
                                     link(std::string("lp-remove-") + r, tx("discs.remove"))
                               : link(std::string("lp-replace-") + r, tx("discs.add"))) +
                 "</div>";
        }
        h += "</div>" + under(r);
    }
    h += look_rml();
    if (!s_note.empty()) h += std::string("<div class='lpnote") + (s_note_ok ? "" : " err") + "'>" + esc(s_note) + "</div>";
    panel->SetInnerRML(h);
    for (int i = 0; i < (int)s_buttons.size(); ++i)
        if (Rml::Element* e = s_doc->GetElementById(s_buttons[i])) e->SetClass("kfocus", i == s_focus);
    if (s_focus >= (int)s_buttons.size()) s_focus = (int)s_buttons.size() - 1;
}

void set_focus(int f) {
    s_focus = f;
    if (s_doc == nullptr) return;
    for (int i = 0; i < (int)s_buttons.size(); ++i)
        if (Rml::Element* e = s_doc->GetElementById(s_buttons[i])) e->SetClass("kfocus", i == s_focus);
    /* the column scrolls while the panel is open (hub.rcss body.langopen #right, 3.5 step 5): the controller's place is
     * kept in view, and back on PLAY the column goes back to its top. All platforms (RmlUi) */
    if (s_focus >= 0 && s_focus < (int)s_buttons.size()) {
        if (Rml::Element* e = s_doc->GetElementById(s_buttons[s_focus]))
            e->ScrollIntoView(Rml::ScrollIntoViewOptions{Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest});
        /* the last place (What changes ›): the column all the way down, so the panel's own bottom edge shows too */
        if (s_focus == (int)s_buttons.size() - 1 && s_focus > 0) {
            if (Rml::Element* col = s_doc->GetElementById("right")) col->SetScrollTop(col->GetScrollHeight());
        }
    } else if (Rml::Element* col = s_doc->GetElementById("right")) {
        col->SetScrollTop(0.f);
    }
}

/* ---- adding ---- */

int copy_thread(void*) {
    if (s_copy_move && SDL_RenamePath(s_copy_src.c_str(), s_copy_dst.c_str())) {
        s_copy_done = s_copy_total.load(); /* the same drive: moved at once */
        pc_file_touch_utf8(s_copy_dst.c_str()); /* a move keeps the old time; the shared rule takes the newest (4.4) */
        s_copy_state = 2;
        return 0;
    }
    SDL_IOStream* in = SDL_IOFromFile(s_copy_src.c_str(), "rb");
    const std::string part = s_copy_dst + ".part";
    SDL_IOStream* out = in != nullptr ? SDL_IOFromFile(part.c_str(), "wb") : nullptr;
    bool ok = in != nullptr && out != nullptr;
    if (ok) {
        std::vector<unsigned char> buf(8u << 20);
        for (;;) {
            const size_t n = SDL_ReadIO(in, buf.data(), buf.size());
            if (n == 0) {
                ok = SDL_GetIOStatus(in) == SDL_IO_STATUS_EOF;
                break;
            }
            if (SDL_WriteIO(out, buf.data(), n) != n) {
                ok = false;
                break;
            }
            s_copy_done += n;
        }
    }
    if (out != nullptr && !SDL_CloseIO(out)) ok = false;
    if (in != nullptr) SDL_CloseIO(in);
    if (ok && !SDL_RenamePath(part.c_str(), s_copy_dst.c_str())) ok = false;
    if (ok && s_copy_move) {
        /* Move across drives: the player's original goes only once the whole copy is in place (as the hub's US disc) */
        SDL_PathInfo a, b;
        if (SDL_GetPathInfo(s_copy_src.c_str(), &a) && SDL_GetPathInfo(s_copy_dst.c_str(), &b) && a.size == b.size)
            SDL_RemovePath(s_copy_src.c_str());
    }
    if (!ok) {
        s_copy_error = SDL_GetError();
        SDL_RemovePath(part.c_str()); /* our own unfinished copy */
    }
    s_copy_state = ok ? 2 : 3;
    return 0;
}

void start_copy(bool move) {
    if (s_ask_path.empty() || s_copy_state == 1) return;
    /* the US disc goes to ROM\, under a name no file has (the open disc's own name can't be overwritten); the others to
     * ROM Other Regions\ */
    const bool us = s_ask_region == 'E';
    const std::string dir = us ? rom_folder() : folder();
    SDL_CreateDirectory(dir.c_str());
    s_copy_src = s_ask_path;
    s_copy_dst = dir + file_name(s_ask_path);
    s_copy_replace.clear();
    if (s_ask == ASK_REPLACE) {
        if (us) s_copy_replace = us_disc();
        else if (const PcLangDisc* old = disc_of(s_ask_region)) s_copy_replace = old->path;
    }
    s_copy_region = s_ask_region;
    if (same_path(s_copy_dst, s_copy_src)) { /* already in the folder (a replace offered from there): nothing to copy */
        s_ask = ASK_NONE;
        s_copy_state = 2;
        return;
    }
    if (us) s_copy_dst = free_name(dir, file_name(s_ask_path));
    s_copy_total = s_ask_size;
    s_copy_done = 0;
    s_copy_move = move;
    s_copy_state = 1;
    s_ask = ASK_NONE;
    std::printf("[langdiscs] %s %s to %s\n", move ? "moving" : "copying", s_copy_src.c_str(), s_copy_dst.c_str());
    SDL_Thread* t = SDL_CreateThread(copy_thread, "hub language disc copy", nullptr);
    if (t == nullptr) {
        s_copy_state = 3;
        s_copy_error = SDL_GetError();
    } else {
        SDL_DetachThread(t);
    }
    render();
}

/* what to say about a file that isn't one of this game's discs */
std::string not_a_colosseum_disc(const char* id) {
    if (id[0] == '\0') return tx("hub.lang.unreadable");
    return tx("hub.discs.not_colosseum", {{"id", id}});
}

/* a file offered (a row's file window, or dropped on the hub): checked; then offered to its own row by its game ID
 * (the US, European or Japanese disc, whichever row it was picked from: Studio does the same), to copy or move in,
 * or to replace the one already there */
void offer(const std::string& path) {
    char id[7];
    const bool readable = pc_lang_disc_identify(path.c_str(), id) != 0;
    if (!readable) id[0] = '\0';
    s_open = true;
    if (!readable || std::strncmp(id, "GC6", 3) != 0 || (id[3] != 'E' && id[3] != 'P' && id[3] != 'J')) {
        set_note(not_a_colosseum_disc(id), false);
        std::printf("[langdiscs] %s: not a Colosseum disc (%s)\n", path.c_str(), readable ? id : "unreadable");
        render();
        return;
    }
    /* a modified disc is refused before anything is copied (snatcherLeo, 2026-11-23: "refuse all three, use the hub's
     * wording"; the same text in Studio's Settings › Discs, hub.lang.not_original then check.verify_hint) */
    if (!pc_lang_disc_is_original(path.c_str(), id)) {
        set_note(tx("hub.lang.not_original", {{"disc", tx(disc_key(id[3]))}}) + " " + tx("check.verify_hint"), false);
        std::printf("[langdiscs] %s: refused, not the original %s\n", path.c_str(), id);
        render();
        return;
    }
    SDL_PathInfo info;
    s_ask_path = path;
    s_ask_size = SDL_GetPathInfo(path.c_str(), &info) ? info.size : 0;
    s_ask_region = id[3];
    const PcLangDisc* have = disc_of(id[3]);
    const std::string have_path = id[3] == 'E' ? us_disc() : have != nullptr ? have->path : "";
    if (!have_path.empty() && same_path(path, have_path)) {
        set_note(tx("hub.lang.already", {{"disc", tx(disc_key(id[3]))}}), true);
        s_ask = ASK_NONE;
    } else {
        s_ask = !have_path.empty() ? ASK_REPLACE : ASK_OFFER;
        set_note("", true);
    }
    std::printf("[langdiscs] offered %s (%s)%s\n", path.c_str(), id, s_ask == ASK_REPLACE ? ": one is already there" : "");
    render();
    /* a test switch: ORRE_HUB_TEST_LANG_ADD=0 / 1 accepts with Copy / Move */
    if (const char* test = SDL_getenv("ORRE_HUB_TEST_LANG_ADD"); test != nullptr && (test[0] == '0' || test[0] == '1'))
        start_copy(test[0] == '1');
}

void SDLCALL picked(void*, const char* const* files, int) {
    if (files != nullptr && files[0] != nullptr) {
        const std::lock_guard lock(s_pick_mutex);
        s_picked = files[0];
    }
}

void browse() {
    static std::string discs_name, all_name;
    discs_name = tx("hub.add.filter.discs");
    all_name = tx("hub.add.filter.all");
    static SDL_DialogFileFilter filters[2];
    filters[0] = {discs_name.c_str(), "iso;gcm;ciso;rvz;gcz;wia"};
    filters[1] = {all_name.c_str(), "*"};
    int n = 0;
    SDL_Window* window = nullptr;
    if (SDL_Window** all = SDL_GetWindows(&n)) {
        window = n > 0 ? all[0] : nullptr;
        SDL_free(all);
    }
    SDL_ShowOpenFileDialog(picked, nullptr, window, filters, 2, nullptr, false);
}

/* the discs changed (added, removed, found by hand): the list, the game's text, the panel */
void changed() {
    refresh_list();
    pc_region_text_rescan();
    render();
}

/* ---- the folders, watched (every two seconds while the hub shows) ---- */

std::string signature(const std::string& dir) {
    std::string sig;
    int count = 0;
    if (char** files = SDL_GlobDirectory(dir.c_str(), "*", 0, &count)) {
        for (int i = 0; i < count; ++i) {
            SDL_PathInfo info;
            if (SDL_GetPathInfo((dir + files[i]).c_str(), &info))
                sig += std::string(files[i]) + ":" + std::to_string(info.size) + ";";
        }
        SDL_free(files);
    }
    return sig;
}

/* a European or Japanese Colosseum disc put into ROM\ by hand goes to ROM Other Regions\ (ROM\ holds only the disc
 * that's played; the project's rule, the project plan "Every way to add a disc"). Files are looked at once, by name and size */
void move_from_rom() {
    static std::string seen;
    const std::string rom = rom_folder();
    const std::string sig = signature(rom);
    if (sig == seen) return;
    seen = sig;
    int count = 0;
    bool moved = false;
    if (char** files = SDL_GlobDirectory(rom.c_str(), "*", 0, &count)) {
        for (int i = 0; i < count; ++i) {
            const std::string path = rom + files[i];
            if (path.size() > 5 && path.compare(path.size() - 5, 5, ".part") == 0) continue;
            char id[7];
            if (!pc_lang_disc_identify(path.c_str(), id) || std::strncmp(id, "GC6", 3) != 0 || (id[3] != 'P' && id[3] != 'J'))
                continue;
            SDL_CreateDirectory(folder().c_str());
            const std::string to = folder() + files[i];
            if (!SDL_GetPathInfo(to.c_str(), nullptr) && SDL_RenamePath(path.c_str(), to.c_str())) {
                std::printf("[langdiscs] %s was in ROM\\: moved to ROM Other Regions\\\n", files[i]);
                set_note(tx("hub.lang.moved_from_rom", {{"disc", tx(disc_key(id[3]))}}), true);
                moved = true;
            }
        }
        SDL_free(files);
    }
    if (moved) changed();
}

} // namespace

void hub_lang_attach(Rml::ElementDocument* doc) {
    s_doc = doc;
    move_from_rom();
    refresh_list();
    /* a test switch for pictures of the panel: ORRE_HUB_TEST_LANG=open starts with it unfolded; "look" also opens the
     * Japanese release look's "What changes" window (3.5 step 5) */
    const char* test = SDL_getenv("ORRE_HUB_TEST_LANG");
    if (test != nullptr && (std::strcmp(test, "open") == 0 || std::strcmp(test, "look") == 0)) s_open = true;
    render();
    if (test != nullptr && std::strcmp(test, "look") == 0) look_window(true);
    /* test switches for the Discs panel (2026-11-23; never settings): ORRE_HUB_TEST_LANG_OFFER=<file> offers that file
     * as if picked in a row (with ORRE_HUB_TEST_LANG_ADD=0 / 1 it is then copied / moved in); ORRE_HUB_TEST_LANG_REMOVE=
     * E, P or J asks that row's Remove question, and ORRE_HUB_TEST_LANG_REMOVE_YES=1 answers it. All platforms */
    if (const char* offer_test = SDL_getenv("ORRE_HUB_TEST_LANG_OFFER")) offer(offer_test);
    if (const char* rm = SDL_getenv("ORRE_HUB_TEST_LANG_REMOVE"); rm != nullptr && std::strchr("EPJ", rm[0]) && rm[0]) {
        s_open = true;
        s_ask = ASK_REMOVE;
        s_ask_region = rm[0];
        render();
        if (SDL_getenv("ORRE_HUB_TEST_LANG_REMOVE_YES") != nullptr) {
            if (Rml::Element* yes = s_doc != nullptr ? s_doc->GetElementById("lp-yes") : nullptr) hub_lang_click(yes);
        }
    }
}

bool hub_lang_is_open() { return s_open; }

void hub_lang_open(bool open) {
    s_open = open;
    if (!open) s_ask = ASK_NONE;
    if (open) refresh_list();
    render();
}

bool hub_lang_click(Rml::Element* target) {
    for (Rml::Element* e = target; e != nullptr; e = e->GetParentNode()) {
        const std::string id = e->GetId();
        if (id == "b-lang") {
            hub_lang_open(!s_open);
            return true;
        }
        if (id.rfind("lp-replace-", 0) == 0) { /* a row's Add… or Replace…: the file window (its pick goes to its own row) */
            set_note("", true);
            browse();
            return true;
        }
        if (id == "lp-jplook") { /* the Japanese release look's "What changes" window (3.5 step 5) */
            look_window(true);
            return true;
        }
        if (id == "look-close" || id == "lookdim") {
            look_window(false);
            return true;
        }
        if (id == "lookbox") return true; /* a click on the window's own background */
        if (id == "lp-copy" || id == "lp-move") {
            start_copy(id == "lp-move");
            return true;
        }
        if (id == "lp-no") {
            s_ask = ASK_NONE;
            set_note("", true);
            render();
            return true;
        }
        if (id.rfind("lp-remove-", 0) == 0) {
            s_ask = ASK_REMOVE;
            s_ask_region = id.back();
            set_note("", true);
            render();
            return true;
        }
        if (id == "lp-yes" && s_ask == ASK_REMOVE) {
            const PcLangDisc* d = disc_of(s_ask_region);
            const std::string disc = tx(disc_key(s_ask_region));
            s_ask = ASK_NONE;
            if (s_ask_region == 'E') { /* the US disc: OrrePort restarts (asks for a disc then) unless it stays */
                us_disc_finish(us_disc(), "Remove", true);
                set_note(tx("discs.remove_failed", {{"disc", disc}}), false);
            } else if (d != nullptr && pc_recycle_file(d->path)) {
                set_note(tx("discs.removed", {{"disc", disc}}), true);
            } else {
                set_note(tx("discs.remove_failed", {{"disc", disc}}), false);
            }
            changed();
            return true;
        }
        if (id == "langpanel") return true; /* a click on the panel's own background */
    }
    return false;
}

bool hub_lang_look_is_open() { return s_look_open; }

/* While the "What changes" window is open it takes every input, as the hub's other windows do: A, B, Enter, Space or
 * Escape close it (it only shows), Up / Down (D-pad, left stick, arrow keys) scroll its list, the mouse clicks Back or
 * outside it and its wheel scrolls (RmlUi). All platforms the same */
aurora::input::EventResult hub_lang_look_event(const aurora::input::InputEvent& event) {
    using E = aurora::input::InputEvent;
    Rml::Element* list = s_doc != nullptr ? s_doc->GetElementById("looklist") : nullptr;
    auto scroll = [list](int d) {
        if (list != nullptr) list->SetScrollTop(list->GetScrollTop() + 60.f * (float)d);
    };
    if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
        if (button->pressed) {
            if (button->button == SDL_GAMEPAD_BUTTON_SOUTH || button->button == SDL_GAMEPAD_BUTTON_EAST) look_window(false);
            else if (button->button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) scroll(1);
            else if (button->button == SDL_GAMEPAD_BUTTON_DPAD_UP) scroll(-1);
        }
        return aurora::input::EventResult::Consume;
    }
    if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
        if (key->pressed) {
            if (!key->repeat && (key->keycode == SDLK_ESCAPE || key->keycode == SDLK_RETURN ||
                                 key->keycode == SDLK_KP_ENTER || key->keycode == SDLK_SPACE)) look_window(false);
            else if (key->keycode == SDLK_DOWN) scroll(1);
            else if (key->keycode == SDLK_UP) scroll(-1);
        }
        return aurora::input::EventResult::Consume;
    }
    if (const auto* axis = event.payload.get_if<E::AxisChanged>()) {
        static float last = 0.f;
        if (axis->axis == SDL_GAMEPAD_AXIS_LEFTY) {
            const int was = last > 0.5f ? 1 : last < -0.5f ? -1 : 0, now = axis->value > 0.5f ? 1 : axis->value < -0.5f ? -1 : 0;
            last = axis->value;
            if (now != 0 && now != was) scroll(now);
        }
        return aurora::input::EventResult::Consume;
    }
    aurora::rmlui::process_input(event); /* the mouse: Back, outside, the wheel */
    return aurora::input::EventResult::Consume;
}

int hub_lang_focus() { return s_focus; }
void hub_lang_set_focus(int f) { set_focus(std::clamp(f, -1, (int)s_buttons.size() - 1)); }
int hub_lang_buttons() { return (int)s_buttons.size(); }

bool hub_lang_event(const aurora::input::InputEvent& event) {
    using E = aurora::input::InputEvent;
    int dir = 0, press = 0, back = 0;
    if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
        if (!button->pressed) return s_focus >= 0 && button->button == SDL_GAMEPAD_BUTTON_SOUTH; /* A's release too */
        if (button->button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) dir = 1;
        else if (button->button == SDL_GAMEPAD_BUTTON_DPAD_UP) dir = -1;
        else if (button->button == SDL_GAMEPAD_BUTTON_SOUTH) press = 1;
        else if (button->button == SDL_GAMEPAD_BUTTON_EAST) back = 1;
    } else if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
        if (!key->pressed || key->repeat) return false;
        if (key->keycode == SDLK_DOWN) dir = 1;
        else if (key->keycode == SDLK_UP) dir = -1;
        else if (s_focus >= 0 && (key->keycode == SDLK_RETURN || key->keycode == SDLK_KP_ENTER || key->keycode == SDLK_SPACE)) press = 1;
    } else {
        return false;
    }
    if (dir != 0) {
        if (s_focus < 0 && dir < 0) return false; /* Up on PLAY: nothing here */
        int f = s_focus + dir;
        if (f < -1) f = -1;
        if (f >= (int)s_buttons.size()) f = (int)s_buttons.size() - 1;
        set_focus(f);
        return true;
    }
    if (press && s_focus >= 0 && s_focus < (int)s_buttons.size()) {
        if (Rml::Element* e = s_doc != nullptr ? s_doc->GetElementById(s_buttons[s_focus]) : nullptr) {
            const std::string was = s_buttons[s_focus];
            hub_lang_click(e);
            /* the same button keeps the focus when it is still there (the link, Cancel's neighbours) */
            int keep = 0;
            for (int i = 0; i < (int)s_buttons.size(); ++i)
                if (s_buttons[i] == was) keep = i;
            set_focus(keep);
        }
        return true;
    }
    if (back && s_open) {
        hub_lang_open(false);
        set_focus(0);
        return true;
    }
    return false;
}

void hub_lang_frame() {
    /* a test switch for pictures of the controller's place: ORRE_HUB_TEST_LANG=focuslook opens the panel and, 2 s
     * after the hub's first frame, puts the place on "What changes ›", as Down would (the column scrolls to it) */
    if (static Uint64 test_at = 0; test_at != ~0ull) {
        const char* test = SDL_getenv("ORRE_HUB_TEST_LANG");
        if (test == nullptr || std::strcmp(test, "focuslook") != 0) {
            test_at = ~0ull;
        } else if (test_at == 0) {
            test_at = SDL_GetTicks() + 2000;
            hub_lang_open(true);
        } else if (SDL_GetTicks() >= test_at) {
            test_at = ~0ull;
            for (int i = 0; i < (int)s_buttons.size(); ++i)
                if (s_buttons[i] == "lp-jplook") set_focus(i);
        }
    }
    {
        std::string path;
        {
            const std::lock_guard lock(s_pick_mutex);
            path.swap(s_picked);
        }
        if (!path.empty()) offer(path);
    }
    if (s_copy_state == 1) {
        static Uint64 last = 0;
        if (SDL_GetTicks() - last > 250) { /* the percentage, four times a second */
            last = SDL_GetTicks();
            render();
        }
        return;
    }
    if (s_copy_state == 2 || s_copy_state == 3) {
        const bool ok = s_copy_state == 2;
        s_copy_state = 0;
        const std::string disc = tx(disc_key(s_copy_region));
        if (ok && s_copy_region == 'E') {
            /* the US disc: stamped newest (the shared rule picks it at the start), then the old one out and a restart */
            pc_file_touch_utf8(s_copy_dst.c_str());
            std::printf("[langdiscs] %s added: %s\n", disc.c_str(), s_copy_dst.c_str());
            us_disc_finish(same_path(s_copy_replace, s_copy_dst) ? std::string() : s_copy_replace, "Replace", false);
        }
        if (ok) {
            if (!s_copy_replace.empty() && s_copy_replace != s_copy_dst) pc_recycle_file(s_copy_replace.c_str());
            set_note(tx(s_copy_region == 'J' ? "hub.lang.added_jp" : "hub.lang.added", {{"disc", disc}}), true);
            std::printf("[langdiscs] %s added: %s\n", disc.c_str(), s_copy_dst.c_str());
        } else {
            set_note(tx(s_copy_move ? "discs.move_failed" : "discs.copy_failed", {{"error", s_copy_error}}), false);
        }
        changed();
        return;
    }
    static Uint64 last = 0;
    if (SDL_GetTicks() - last < 2000) return;
    last = SDL_GetTicks();
    move_from_rom();
    static std::string seen = "?";
    const std::string sig = signature(folder());
    if (sig != seen) {
        const bool first = seen == "?";
        seen = sig;
        if (!first) changed(); /* a disc put in or taken out of ROM Other Regions\ by hand */
    }
}

bool hub_lang_drop(const char* path) {
    char id[7];
    if (path == nullptr || !pc_lang_disc_identify(path, id)) return false;
    /* a US disc dropped while one is open goes to the US row's Replace (with none open, the hub's own add flow takes
     * it: the blank disc's drop) */
    if (std::strncmp(id, "GC6", 3) != 0 || (id[3] != 'P' && id[3] != 'J' && (id[3] != 'E' || us_disc().empty())))
        return false;
    offer(path);
    return true;
}
