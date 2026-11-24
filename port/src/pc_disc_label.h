/*
 * pc_disc_label.h: the hub disc's label as the player chooses it (2026-11-13; snatcherLeo: "Yes, build it", mockup 47's
 * label shelf). OrrePort's own label, the game's banner, or any picture of the player's: the pictures are kept in
 * OrrePort's "Disc labels" folder, each with where it sits on the disc (zoom and offset), and each game remembers which
 * label it wears. This file holds the data and the drawing; the shelf page itself is in pc_hub.cpp.
 *
 * Why its own file: the drawing (fitting a picture into the disc's round label, under the rim and the clear centre)
 * and the folder bookkeeping have nothing to do with the hub's page logic, and Studio's future label tools can reuse
 * them. Platform-neutral: plain C++ and SDL (files, PNG reading); nothing Windows-only (Linux / macOS: the same,
 * likely).
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace disc_label {

/* a picture: RGBA8, straight (not premultiplied) alpha, rows top to bottom */
struct Rgba {
    int w = 0, h = 0;
    std::vector<unsigned char> px;
};

/* where a picture sits on the disc: zoom 0 fills the disc exactly (the picture's shorter side spans the label's
 * width), 100 is 4 times that, -100 a quarter; x / y move the picture's centre from the disc's centre, in label units
 * (the label is 420 units across, as the label script draws it); rot turns it. *Changed 2026-11-15:* it was kept so no
 * edge ever showed (zoom 0 at least, edges outside the disc); now a picture may be smaller than the disc, turned, or
 * moved until only part of it is on the disc (clamp), the rest showing the disc's own black */
struct Place {
    float zoom = 0.0f;
    float x = 0.0f, y = 0.0f;
    /* *Added 2026-11-15 (snatcherLeo: "let users zoom out even further ... and/or wanna rotate", mockup A):* the turn in
     * degrees, -180 to 180 (positive: clockwise); zoom now runs -100 (a quarter size) to 100 (4x), 0 still filling the
     * disc */
    float rot = 0.0f;
};

struct Entry {
    std::string file; /* its name in the Disc labels folder */
    Place place;
    uint64_t used = 0; /* when it was last worn: a counter, bigger is more recent (the shelf shows the newest first) */
};

/* the printed area's radius in label units: the label is 420 across, the rim takes 204-210 (disc_label.py) */
constexpr float kLabelRadius = 204.0f;

std::string folder(); /* <OrrePort>/Disc labels/ (made when first needed) */
void load();          /* labels.txt, plus any picture put in the folder by hand; forgets pictures that are gone */
void save();
std::vector<Entry>& entries();
Entry* find(const std::string& file);
uint64_t next_used();

/* which label a game wears: "orreport", "banner" or "picture:<file>" ("orreport" when nothing is chosen) */
std::string wearing(const char* game);
void set_wearing(const char* game, const std::string& value);

/* the picture formats it reads: PNG and BMP always (SDL), JPG, GIF (its first frame), TGA and PSD with stb_image, WebP
 * with libwebp (both approved 2026-11-13); why says what went wrong, in the player's words */
bool decode(const std::string& path, Rgba& out, std::string& why);
bool is_picture_name(const std::string& name); /* by extension: what decode reads */
std::string formats_text();                    /* "PNG, JPG, ... or BMP": what this build reads, for the page */
std::vector<std::string> dialog_extensions();  /* the same, for the file dialog's filter */
void shrink(const Rgba& in, int max_side, Rgba& out); /* an area-averaged smaller copy (a plain copy if it fits) */

/* a picture from anywhere: copied into the folder (the original stays where it is); file gets its name there. name:
 * what the player called it on Save (snatcherLeo, 2026-11-13: "after I click save, a title box will open to allow me to name
 * it"); the file is "<name>.png" (or "<name> (2).png" if that is taken); empty keeps the original's name */
bool add(const std::string& source, std::string& file, std::string& why, const std::string& name = "");
/* a new name for a picture on the shelf: the file is renamed to "<name>.png"; file comes back as the new file name */
bool rename(const std::string& old_file, const std::string& name, std::string& file, std::string& why);
/* a name as the player typed it, made safe for a file name on every OS (no / \ : * ? " < > |, no leading or trailing
 * spaces or dots, at most 60 characters); empty if nothing usable is left */
std::string clean_name(const std::string& typed);
/* off the shelf: moved into the folder's Removed\ (not deleted, so it can be brought back) */
bool remove(const std::string& file);

float fill_scale(const Rgba& pic);              /* label units per picture pixel at zoom 0 */
float scale(const Rgba& pic, const Place& p);   /* ... at p's zoom */
void clamp(const Rgba& pic, Place& p);          /* zoom to -100..100, the turn to -180..180, some of it on the disc */
float fit_zoom(const Rgba& pic);                /* the zoom at which the whole picture, at any turn, lies inside the disc */

/* the label drawn at frame's size: the picture placed as p says inside the printed circle, then frame (the rim and the
 * clear centre, drawn from OrrePort's own label with its printing left out) over it. The hub adds the GameCube's hole
 * and the margin afterwards, as for its own label. */
void compose_picture(const Rgba& pic, const Place& p, const Rgba& frame, Rgba& out);
/* the game's banner (96 x 32 RGBA8): a black disc with the banner across its upper half, pixels kept sharp */
void compose_banner(const unsigned char* banner, const Rgba& frame, Rgba& out);

/* the template (the approved draft's "1024 x 1024 template"): a picture made from it fills the disc exactly at zoom 0,
 * with nothing to move (snatcherLeo: "if it's a perfect fit (the image they upload is the template) then, they wouldn't have
 * much to move around"). Saved as Disc labels\Template.png; the path comes back in path. */
bool save_template(std::string& path, std::string& why);

} // namespace disc_label
