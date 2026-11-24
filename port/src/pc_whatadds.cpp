/*
 * pc_whatadds.cpp: the "What OrrePort adds" list as RML, for its window in Settings › About (5.13, 2026-11-24).
 *
 * Why: the public page of everything OrrePort changes from the original game (docs/public-repo/what-orreport-adds.md,
 * approved by snatcherLeo 2026-11-24) belongs inside the app too, so a player can check "is this already in?" without leaving
 * it. It shows in the same scrolling window as "What it turns off" (pc_settings_overlay.cpp, #whatdlg).
 *
 * How: ui/adds.txt gives the order (sections, rows, notes; the UI Editor can change it later, snatcherLeo's 2026-11-15 rule
 * for every list OrrePort adds) and every word is a ui/lang key (adds.*), so the list reads in each language and new
 * languages only need their file. The file is read once per open (it is small), so an edited file shows at once.
 * Text keeps the language files' rule (no "<" or "&"), so it goes in as written, like hub_accurate_list_rml.
 *
 * Platforms: plain C++ (std::ifstream); the same on Windows, Linux and macOS (likely; only Windows built).
 * XD: the same window and file format; XD would get its own list file (guess: ui/adds_xd.txt, chosen by the game).
 */
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "pc_paths.h"
#include "pc_text.h"

std::string pc_whatadds_rml() {
    std::string out = "<div class='awi'>" + tx("adds.intro") + "</div>";
    std::ifstream in(pc_paths_str(PC_PLACE_UI, "adds.txt"));
    if (!in) {
        std::printf("[whatadds] no ui/adds.txt: the list shows its introduction only\n");
        return out;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream f(line);
        std::string kind, id, sub, where, acc;
        f >> kind >> id >> sub >> where >> acc;
        const std::string k = "adds." + id;
        if (kind == "section") {
            out += "<div class='awh k2'>" + tx(("adds.sec." + id).c_str()) + "</div>";
        } else if (kind == "note") {
            out += "<div class='adn'>" + tx(k.c_str()) + "</div>";
        } else if (kind == "row") {
            out += "<div class='adr'><span class='adt'>" + tx(k.c_str()) + "</span>";
            if (sub == "sub") out += "<span class='ads'>" + tx((k + ".sub").c_str()) + "</span>";
            std::string status;
            if (where == "always") status = tx("adds.where.always");
            else if (where == "own") status = tx((k + ".where").c_str());
            std::string a;
            if (acc == "off" || acc == "same" || acc == "look") a = tx(("adds.acc." + acc).c_str());
            else if (acc == "own") a = tx((k + ".acc").c_str());
            if (!a.empty()) status += (status.empty() ? "" : " · ") + a;
            if (!status.empty()) out += "<span class='adm'>" + status + "</span>";
            out += "</div>";
        } else {
            std::printf("[whatadds] ui/adds.txt: unknown line \"%s\"\n", line.c_str());
        }
    }
    return out;
}
