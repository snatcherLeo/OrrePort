// species.cpp: the species table, its mug shots, PC pictures and battle models (port of tools/local/colo/tables.py and
// the Pokémon part of tools/local/extract.py; layouts: docs/engine/pokemon-data.md, game-data-tables.md, textures.md).
// Why: the Pokémon editor's first data. Read field by field from the big-endian record (the review's decision 3); the
// offsets are checked against the decomp's PokemonData at compile time in layout_check.c. Platforms: plain C++.
#include <algorithm>
#include <cstdio>

#include "internal.hpp"

namespace orre {

// common_rel: the game's REL data module, entry "common_rel" of common.fsys, unpacked
const Bytes& Disc::rel() {
    std::lock_guard<std::recursive_mutex> g(lock);
    if (!rel_loaded) {
        rel_loaded = true;
        Bytes arc;
        if (read_archive("common", arc))
            for (const auto& e : fsys_entries(arc))
                if (e.name == "common_rel") { common_rel = fsys_load(arc, e); break; }
    }
    return common_rel;
}

// One table in common_rel's section 4: rows of `size` bytes at `off`, the count a u32 at `count_off` (tables.py
// table_rows; the module's prolog fills the game's pointers and counts from these same places).
std::vector<const uint8_t*> table_rows(const Bytes& rel, uint32_t off, uint32_t count_off, uint32_t size) {
    std::vector<const uint8_t*> rows;
    const uint32_t count = be32(rel, kSec4 + count_off);
    for (uint32_t i = 0; i < count && kSec4 + off + (size_t)(i + 1) * size <= rel.size(); ++i)
        rows.push_back(rel.data() + kSec4 + off + (size_t)i * size);
    return rows;
}

namespace {
constexpr uint32_t kSpeciesOff = 0x121a04, kSpeciesCount = 0x14a6e0, kSpeciesSize = 0x11C;
constexpr uint32_t kMenuFacesOff = 0x44, kMenuFacesCount = 0x14a65c, kMenuFacesSize = 8;
// main.dol tables (extract.py cmd_pokemon): the Pokémon-model table {archive id, entry id, name msg} x 12 bytes at
// lbl_80370BD0 (0x1390 bytes), and the shiny-model override table at lbl_80374F20 (0x1A8 bytes: {u16 species, u16 0},
// archive id, entry id).
constexpr uint32_t kPkxTable = 0x80370BD0, kPkxTableSize = 0x1390;
constexpr uint32_t kRareTable = 0x80374F20, kRareTableSize = 0x1A8;

const uint8_t* species_row(Disc& d, int index) {
    auto rows = table_rows(d.rel(), kSpeciesOff, kSpeciesCount, kSpeciesSize);
    return index >= 0 && index < (int)rows.size() ? rows[index] : nullptr;
}

// Every type-9 texture of poke_face.fsys and poke_body.fsys by its entry id (mug-shot sheets and PC pictures)
}  // namespace

void Disc::load_face_textures() {
    std::lock_guard<std::recursive_mutex> g(lock);
    if (face_tex_done) return;
    face_tex_done = true;
    for (const char* name : {"poke_face", "poke_body"}) {
        const Bytes* arc = archive(name);
        if (!arc) continue;
        for (const auto& e : fsys_entries(*arc))
            if (e.type == 9) face_tex[e.id] = {name, e};  // later archives win, as extract.py's dict does
    }
}

namespace {
bool face_texture(Disc& d, uint32_t id, int& w, int& h, std::vector<uint8_t>& rgba) {
    d.load_face_textures();
    auto it = d.face_tex.find(id);
    if (it == d.face_tex.end()) return false;
    const Bytes* arc = d.archive(it->second.first);
    if (!arc) return false;
    return decode_texture_file(fsys_load(*arc, it->second.second), w, h, rgba);
}

void to_image(OrreImage* out, int w, int h, const std::vector<uint8_t>& rgba, int x0, int y0, int cw, int ch) {
    out->width = cw;
    out->height = ch;
    out->rgba = static_cast<uint8_t*>(std::malloc((size_t)cw * ch * 4));
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x) {
            const int sx = x0 + x, sy = y0 + y;
            uint8_t* o = out->rgba + ((size_t)y * cw + x) * 4;
            if (sx < w && sy < h) std::memcpy(o, &rgba[((size_t)sy * w + sx) * 4], 4);
            else std::memset(o, 0, 4);
        }
}
}  // namespace

// The archive holding a model: GSfsys.toc maps archive ids to names; the entry is found by its id (tables.py find_entry)
// (shared with wzx.cpp, which reads the move-effect files the same way)
bool find_entry(Disc& d, uint32_t archive_id, uint32_t entry_id, Bytes& out) {
    d.load_toc();
    auto it = d.toc.find(archive_id);
    if (it == d.toc.end()) return false;
    Bytes arc;
    if (!d.read_archive(it->second, arc)) return false;
    for (const auto& e : fsys_entries(arc))
        if (e.id == entry_id) { out = fsys_load(arc, e); return true; }
    return false;
}

}  // namespace orre

using namespace orre;

extern "C" int orre_species_count(OrreDisc* od) {
    if (!od) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    return (int)table_rows(od->d.rel(), kSpeciesOff, kSpeciesCount, kSpeciesSize).size();
}

// Evolution kinds grouped (orre_core.h; the table: docs/engine/evolution.md, verified from pokemonEvolutionCheck).
// Plain tables, no disc access: the same on every platform, and for XD the same Gen 3 ids (likely; XD's own
// additions not checked).
extern "C" OrreEvoCategory orre_evo_category(int kind) {
    switch (kind) {
        case 4: case 13: return ORRE_EVO_LEVEL;
        case 1: case 2: case 3: return ORRE_EVO_FRIENDSHIP;
        case 5: case 6: return ORRE_EVO_TRADE;
        case 7: return ORRE_EVO_ITEM;
        case 8: case 9: case 10: return ORRE_EVO_STATS;
        case 11: case 12: return ORRE_EVO_PERSONALITY;
        case 14: return ORRE_EVO_EXTRA;
        case 15: return ORRE_EVO_BEAUTY;
        default: return ORRE_EVO_NONE;
    }
}
extern "C" OrreEvoTrigger orre_evo_trigger(int kind) {
    if (kind == 5 || kind == 6) return ORRE_EVO_ON_TRADE;  // mode 2, getEvoPokemonTrade
    if (kind == 7) return ORRE_EVO_ON_ITEM;                // mode 1, getEvoPokemonItem
    if (kind >= 1 && kind <= 15) return ORRE_EVO_ON_LEVEL_UP;  // mode 0, getEvoPokemonLevelUp
    return ORRE_EVO_ON_NONE;
}
extern "C" OrreEvoParam orre_evo_param(int kind) {
    if (kind == 6 || kind == 7) return ORRE_EVO_PARAM_ITEM;
    if (kind == 4 || (kind >= 8 && kind <= 14)) return ORRE_EVO_PARAM_LEVEL;
    if (kind == 15) return ORRE_EVO_PARAM_VALUE;
    return ORRE_EVO_PARAM_NONE;
}
extern "C" int orre_evo_works_in_colosseum(int kind) {
    return kind >= 1 && kind <= 15 && kind != 2 && kind != 3 && kind != 13 ? 1 : 0;
}

extern "C" int orre_species_raw(OrreDisc* od, int index, uint8_t out[0x11C]) {
    if (!od) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    const uint8_t* r = species_row(od->d, index);
    if (!r) return 0;
    std::memcpy(out, r, kSpeciesSize);
    return 1;
}

extern "C" int orre_species_get(OrreDisc* od, int index, OrreSpecies* s) {
    uint8_t r[0x11C];
    if (!s || !orre_species_raw(od, index, r)) return 0;
    *s = OrreSpecies{};
    s->index = index;
    s->growth_rate = r[0x00];
    s->catch_rate = r[0x01];
    s->gender_ratio = r[0x02];
    s->exp_yield = be16(r + 0x06);
    s->base_friendship = be16(r + 0x08);
    s->height = be16(r + 0x0A);
    s->weight = be16(r + 0x0C);
    s->cry = be16(r + 0x0E);
    s->national_dex = be16(r + 0x12);
    s->hoenn_first = be16(r + 0x14);
    s->name_msg = be32(r + 0x18);
    s->category_msg = be32(r + 0x1C);
    s->dex_text_msg = be32(r + 0x20);
    s->model_id = be32(r + 0x2C);
    for (int i = 0; i < 2; ++i) {
        s->types[i] = r[0x30 + i];
        s->abilities[i] = r[0x32 + i];
        s->held_items[i] = be16(r + 0x70 + 2 * i);
    }
    for (int i = 0; i < 6; ++i) {
        s->base_stats[i] = be16(r + 0x84 + 2 * i);
        s->effort[i] = be16(r + 0x90 + 2 * i);
    }
    for (int i = 0; i < 5; ++i) {
        const uint8_t* e = r + 0x9C + 6 * i;
        s->evolutions[i].kind = e[0];
        s->evolutions[i].param = be16(e + 2);
        s->evolutions[i].target = be16(e + 4);
    }
    for (int i = 0; i < 20; ++i) {
        const uint8_t* e = r + 0xBA + 4 * i;
        s->level_moves[i].level = e[0];
        s->level_moves[i].move = be16(e + 2);
    }
    // the learnset's other parts (the Moves tab): the TM/HM bytes and the egg moves (PokemonData wazaMcn / kowaza)
    for (int i = 0; i < 58; ++i) s->machines[i] = r[0x34 + i];
    for (int i = 0; i < 8; ++i) s->egg_moves[i] = be16(r + 0x74 + 2 * i);
    for (int i = 0; i < 2; ++i) {
        const uint8_t* e = r + 0x10C + 8 * i;
        s->faces[i].colour = e[0];
        s->faces[i].mug_shot_id = be16(e + 2);
        s->faces[i].pc_picture_id = be32(e + 4);
    }
    return 1;
}

// Mug shots: species face[shiny].mug_shot_id -> menu-face record (common_rel +0x44, 8 bytes: u8 flags, bit 7 = the
// shiny row; u32 entry id in poke_face.fsys) -> a sheet of 42 x 42 cells; y 42 = the shiny row when bit 7 is set and
// the sheet is taller (verified from the code: gs_range_80109C88.c, extract.py's evidence). x 42, where the sheet is
// that wide, is the same face turned the other way (31 species; orre_core.h).
namespace {
// Unown's per-letter tables in main.dol (addresses from symbols.txt, uses in the decomp; verified from the code)
constexpr uint32_t kUnownMugTable = 0x8035B478;    // u16 [form][shiny] mug-shot ids (gs_range_80109C88.c)
constexpr uint32_t kUnownBodyTable = 0x802676F0;   // u32 [form][shiny] PC-picture entry ids (textures.md)
constexpr uint32_t kUnownModelTable = 0x80374E40;  // {u32 archive id, u32 entry id} [form] (sequence.c)
constexpr uint32_t kUnownFormCount = 0x80478CF0;   // u32: the number of letters the model code wraps on
constexpr int kUnown = 201;
// Castform (species 0x181): its weather looks are entries of the Pokémon-model table, 0x19F / 0x19E / 0x1A0 for the
// battle's forms 1 / 2 / 4 (fightOutPokemonCreateSequence, fight_out_pokemon_suffix_80207C6C.c; verified from the
// code). Studio orders them by their archives' suffixes, _f (fire: sunny), _w (water: rainy), _i (ice: snowy): the
// letters are verified from the archive names, their weather meaning is likely. No separate pictures exist for them.
constexpr int kCastform = 385;
constexpr uint32_t kCastformModels[3] = {0x19F, 0x19E, 0x1A0};
constexpr uint32_t kPkxTableAddr = 0x80370BD0;

// the model-table entry for a Castform form (0 = the species' own entry), ordered _f, _w, _i
uint32_t castform_model(Disc& d, int form) {
    if (form <= 0 || form > 3) return 0;
    d.load_toc();
    static const char* const kOrder[3] = {"_f", "_w", "_i"};
    for (uint32_t id : kCastformModels) {
        Bytes t;
        if (!d.dol_read(kPkxTableAddr + 12 * id, 12, t)) continue;
        auto it = d.toc.find(be32(t, 0));
        if (it == d.toc.end()) continue;
        const std::string& n = it->second;
        if (n.size() >= 2 && n.compare(n.size() - 2, 2, kOrder[form - 1]) == 0) return id;
    }
    return 0;
}
}  // namespace

extern "C" int orre_species_form_archive(OrreDisc* od, int species, int form, char* out, int size) {
    if (!od || !out || size <= 0) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    uint32_t arc = 0;
    Bytes t;
    if (species == kUnown) {
        if (form >= 0 && form < orre_species_form_count(od, species) && od->d.dol_read(kUnownModelTable + 8 * (uint32_t)form, 8, t))
            arc = be32(t, 0);
    } else {
        const uint8_t* r = species_row(od->d, species);
        uint32_t id = species == kCastform && form > 0 ? castform_model(od->d, form) : (r ? be32(r + 0x2C) : 0);
        if (form != 0 && !(species == kCastform && id)) return 0;
        if (id && od->d.dol_read(kPkxTableAddr + 12 * id, 12, t)) arc = be32(t, 0);
    }
    od->d.load_toc();
    auto it = od->d.toc.find(arc);
    if (!arc || it == od->d.toc.end()) return 0;
    std::snprintf(out, (size_t)size, "%s", it->second.c_str());
    return 1;
}

// The model id a form is known by (2026-11-19, the Moves tab's form picker): what the per-Pokémon move-effect table
// keys on. Castform's weather forms have their own entries (0x19F / 0x19E / 0x1A0), each with its own versions of the
// weather moves (verified from the table); Unown's letters come from their own table, not the model-id table, so they
// keep the species' id (likely: the battle's effect lookup takes the owner's model id, and Unown has no own versions).
extern "C" uint32_t orre_species_form_model_id(OrreDisc* od, int species, int form) {
    if (!od) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    if (species == kCastform && form > 0) return castform_model(od->d, form);
    const uint8_t* r = species_row(od->d, species);
    return r ? be32(r + 0x2C) : 0;
}

extern "C" int orre_species_form_count(OrreDisc* od, int species) {
    if (!od) return 0;
    if (species == kCastform) return 4;
    if (species != kUnown) return 1;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    Bytes t;
    if (!od->d.dol_read(kUnownFormCount, 4, t)) return 1;
    const uint32_t n = be32(t, 0);
    return n >= 1 && n <= 28 ? (int)n : 1;  // the three tables are sized for 28 (0x70 / 0xE0 / 0xE0 bytes)
}

static int mug_shot_rect(OrreDisc* od, int species, int form, int shiny, int x, int width, OrreImage* out) {
    if (!od || !out) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    const uint8_t* r = species_row(od->d, species);
    if (!r) return 0;
    unsigned pid = be16(r + 0x10C + 8 * (shiny ? 1 : 0) + 2);
    if (species == kUnown) {
        // Unown's species record has no mug shot of its own (id 201 points at no sheet; the list showed a blank):
        // the game reads its per-letter table instead
        Bytes t;
        if (form < 0 || form >= orre_species_form_count(od, species) ||
            !od->d.dol_read(kUnownMugTable + 4 * (uint32_t)form + 2 * (shiny ? 1 : 0), 2, t))
            return 0;
        pid = be16(t, 0);
    } else if (form != 0 && species != kCastform) {
        return 0;  // (Castform's weather forms have no mug shots of their own: they show its usual one)
    }
    auto faces = table_rows(od->d.rel(), kMenuFacesOff, kMenuFacesCount, kMenuFacesSize);
    if (!pid || pid >= faces.size()) return 0;
    const uint8_t* rec = faces[pid];
    int w, h;
    std::vector<uint8_t> rgba;
    if (!face_texture(od->d, be32(rec + 4), w, h, rgba)) return 0;
    const int y = (rec[0] & 0x80) && h > 42 ? 42 : 0;
    if (x + width > w) return 0;
    to_image(out, w, h, rgba, x, y, width, 42);
    return 1;
}

extern "C" int orre_species_mug_shot(OrreDisc* od, int species, int shiny, int cell, OrreImage* out) {
    return mug_shot_rect(od, species, 0, shiny, cell ? 42 : 0, 42, out);
}

extern "C" int orre_species_form_mug_shot(OrreDisc* od, int species, int form, int shiny, int cell, OrreImage* out) {
    return mug_shot_rect(od, species, form, shiny, cell ? 42 : 0, 42, out);
}

extern "C" int orre_species_pc_picture(OrreDisc* od, int species, int shiny, OrreImage* out) {
    return orre_species_form_pc_picture(od, species, 0, shiny, out);
}

// PC pictures: species face[shiny].pc_picture_id is the entry id of a poke_body.fsys texture ("_c" files are shiny);
// Unown's come from its per-letter table
extern "C" int orre_species_form_pc_picture(OrreDisc* od, int species, int form, int shiny, OrreImage* out) {
    if (!od || !out) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    const uint8_t* r = species_row(od->d, species);
    if (!r) return 0;
    uint32_t body = be32(r + 0x10C + 8 * (shiny ? 1 : 0) + 4);
    if (species == kUnown) {
        Bytes t;
        if (form < 0 || form >= orre_species_form_count(od, species) ||
            !od->d.dol_read(kUnownBodyTable + 8 * (uint32_t)form + 4 * (shiny ? 1 : 0), 4, t))
            return 0;
        body = be32(t, 0);
    } else if (form != 0 && species != kCastform) {
        return 0;
    }
    int w, h;
    std::vector<uint8_t> rgba;
    if (!body || !face_texture(od->d, body, w, h, rgba)) return 0;
    to_image(out, w, h, rgba, 0, 0, w, h);
    return 1;
}

// ---- abilities and the type chart (main.dol tables; addresses from config/GC6E01/symbols.txt, layouts from the
// decomp's headers and docs/engine/battle-types.md, battle-abilities.md) ----
namespace {
constexpr uint32_t kAbilityTable = 0x8035F5E0, kAbilityTableSize = 0x3A8, kAbilitySize = 12;
constexpr uint32_t kTypeTable = 0x8035B500, kTypeSize = 0x2C, kTypeCount = 18;
}  // namespace

extern "C" int orre_ability_count(OrreDisc* od) { return od ? (int)(kAbilityTableSize / kAbilitySize) : 0; }

extern "C" int orre_ability_messages(OrreDisc* od, int ability, uint32_t* name_msg, uint32_t* doc_msg) {
    if (!od || ability < 0 || ability >= orre_ability_count(od)) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    Bytes t;
    if (!od->d.dol_read(kAbilityTable + kAbilitySize * (uint32_t)ability, kAbilitySize, t)) return 0;
    if (name_msg) *name_msg = be32(t, 4);
    if (doc_msg) *doc_msg = be32(t, 8);
    return 1;
}

extern "C" int orre_type_effect(OrreDisc* od, int attacker, int defender) {
    if (!od || attacker < 0 || defender < 0 || attacker >= (int)kTypeCount || defender >= (int)kTypeCount) return -1;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    Bytes t;
    if (!od->d.dol_read(kTypeTable + kTypeSize * (uint32_t)attacker + 8 + 2 * (uint32_t)defender, 2, t)) return -1;
    switch (be16(t, 0)) {
        case 0x41: return 20;
        case 0x42: return 5;
        case 0x43: return 0;
        default: return 10;
    }
}

extern "C" uint32_t orre_item_name_msg(OrreDisc* od, int item) {
    if (!od || item < 0) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    Bytes n1, n2, slot, rec;
    if (!od->d.dol_read(0x80478BD8, 4, n1) || !od->d.dol_read(0x80478BB0, 4, n2)) return 0;
    if ((uint32_t)item >= be32(n1, 0) || !od->d.dol_read(0x803681E8 + 2 * (uint32_t)item, 2, slot)) return 0;
    const uint32_t s = be16(slot, 0);
    if (s >= be32(n2, 0) || !od->d.dol_read(0x80363CE8 + s * 0x28 + 0x10, 4, rec)) return 0;
    return be32(rec, 0);
}

extern "C" int orre_type_category(OrreDisc* od, int type) {
    if (!od || type < 0 || type >= (int)kTypeCount) return -1;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    Bytes t;
    if (!od->d.dol_read(kTypeTable + kTypeSize * (uint32_t)type, 1, t)) return -1;
    return t[0];
}

extern "C" void orre_image_free(OrreImage* img) {
    if (!img) return;
    std::free(img->rgba);
    img->rgba = nullptr;
    img->width = img->height = 0;
}

// Battle models: species model_id -> main.dol's Pokémon-model table -> {archive id, entry id} -> a PKX file, whose
// HSD model starts at 0x40 (pkx.py). Shiny: the override table lbl_80374F20 names a separate model for 35 species
// (fn_801DE190 loads it instead of recolouring); the others are recoloured by the game at draw time (not applied yet).
extern "C" int orre_species_model(OrreDisc* od, int species, int shiny, OrreModel* out) {
    return orre_species_form_model(od, species, 0, shiny, out);
}

extern "C" int orre_species_form_model(OrreDisc* od, int species, int form, int shiny, OrreModel* out) {
    return orre_species_form_model_from(od, od, species, form, shiny, out);
}

// A region's own model (the Styles tab, 2026-11-22; snatcherLeo: test "with Jynx switching from normal to Japanese model"): the
// file found through `od`'s tables (the US disc: the core reads main.dol at US addresses, so another region's tables
// can't be read yet), then read from `fd`, another region's disc, by the same archive and entry ids. Verified for Jynx
// (pkx_rougela): the Japanese disc numbers its archives as the US one does (likely for every archive: the game's code
// names them by number). fd == od is the ordinary case
extern "C" int orre_species_form_model_from(OrreDisc* od, OrreDisc* fd, int species, int form, int shiny, OrreModel* out) {
    if (!od || !fd || !out) return 0;
    if (form < 0 || form >= orre_species_form_count(od, species)) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    std::unique_lock<std::recursive_mutex> gf(fd->d.lock, std::defer_lock);
    if (fd != od) gf.lock();
    const uint8_t* r = species_row(od->d, species);
    if (!r) return 0;
    uint32_t arc_id = 0, entry_id = 0;
    // the model-table entry: the species' own, or a Castform weather form's (the shiny table is keyed by the same
    // number, as wazaSequencePokemonCreate looks it up by the model it was asked for)
    const uint32_t model_index = species == kCastform && form > 0 ? castform_model(od->d, form) : be32(r + 0x2C);
    if (species == kCastform && form > 0 && !model_index) return 0;
    if (shiny) {
        Bytes t;
        if (od->d.dol_read(kRareTable, kRareTableSize, t))
            for (size_t i = 0; i + 12 <= t.size(); i += 12)
                if (be32(t, i) >> 16 == (species == kUnown ? (uint32_t)species : model_index) && species != 0) {
                    arc_id = be32(t, i + 4);
                    entry_id = be32(t, i + 8);
                }
    }
    Bytes pkx;
    if (!arc_id && species == kUnown) {
        // Unown (species 0xC9) has no model id: the game picks a model per letter form from lbl_80374E40[form]
        // ({archive id, entry id}; sequence.c, verified from the code). *Corrected 2026-11-18:* until today Studio
        // opened pkx_unknown_a by name for form A; it now reads the game's own table for every letter.
        Bytes t;
        if (od->d.dol_read(kUnownModelTable + 8 * (uint32_t)form, 8, t)) {
            arc_id = be32(t, 0);
            entry_id = be32(t, 4);
        }
    }
    if (pkx.empty()) {
        if (!arc_id) {
            const uint32_t pid = model_index;
            Bytes t;
            if (!pid || pid >= kPkxTableSize / 12 || !od->d.dol_read(kPkxTable + 12 * pid, 12, t)) return 0;
            arc_id = be32(t, 0);
            entry_id = be32(t, 4);
        }
        if (!find_entry(fd->d, arc_id, entry_id, pkx)) return 0;  // (fd: the region's disc the file is read from)
    }
    if (pkx.size() < 0x40) return 0;
    const uint32_t model_size = be32(pkx, 0);
    if (0x40 + (size_t)model_size > pkx.size()) return 0;
    Bytes dat(pkx.begin() + 0x40, pkx.begin() + 0x40 + model_size);
    if (!parse_hsd_model(dat, *out)) return 0;
    // The idle animation: slot 0 of the PKX's slot table (docs/engine/models-animation.md: slot 0 is the idle loop,
    // likely; it is the code's fallback slot and has one loop span). Layout (pkx.py): 0x40 header, the model and the
    // particle bank each padded to 32 bytes, then 0xD0-byte slot records; a record's {type, value} pairs at +0x8C,
    // type 0 = "play animation number value".
    const uint32_t gpt1_size = be32(pkx, 4), slots = be32(pkx, 8);
    out->particle_bank_size = (int)gpt1_size;
    out->model_id = (int)model_index;  // (the owner's +0x70: the battle camera gives model 0x13A, Wailord, its wide shot)
    size_t o = 0x40 + ((model_size + 0x1F) & ~0x1Fu);
    // the PKX's own particle bank right after the model (its ambient effects: models-animation.md, 12 species). The
    // reader gets the rest of the file, not only the bank's stated size: Gastly's id table runs 0x2C bytes past it
    // (0xE60 + 23 x 4 > 0xE90; verified 2026-11-19), and the game, which loads the whole PKX, reads those bytes
    if (gpt1_size && o + gpt1_size <= pkx.size()) out->particles = orre_particle_bank_read(pkx.data() + o, pkx.size() - o);
    if (gpt1_size) o += (gpt1_size + 0x1F) & ~0x1Fu;
    // every slot's animation (the Moves tab, 2026-11-19): each 0xD0-byte record's first {type 0, value} pair
    out->slot_count = (int)std::min<uint32_t>(slots, 17);
    out->size_class = (int)be32(pkx, 0x0C);  // the size class (attach-points.md "Two more header fields")
    for (int s = 0; s < 17; ++s) out->slot_animation[s] = -1;
    for (int s = 0; s < out->slot_count; ++s) {
        const size_t r = o + (size_t)s * 0xD0;
        if (r + 0xD0 > pkx.size()) break;
        // its time points (+0x0C, 60 Hz frames, +0x00 how many) and attach points (+0x4C), for move effects and the
        // battle camera (release step 4)
        out->slot_time_count[s] = (int)std::min<uint32_t>(be32(pkx, r), 16);
        for (int k = 0; k < 16; ++k) {
            out->slot_times[s][k] = (int)be32(pkx, r + 0x0C + 4 * (size_t)k);
            out->slot_points[s][k] = (int)be32(pkx, r + 0x4C + 4 * (size_t)k);
        }
        const uint32_t n = std::min<uint32_t>(be32(pkx, r + 4), 8);
        for (uint32_t k = 0; k < n; ++k)
            if (be32(pkx, r + 0x8C + 8 * k) == 0) {
                const int a = (int)be32(pkx, r + 0x90 + 8 * k);
                if (a >= 0 && a < out->animation_count) out->slot_animation[s] = a;
                break;
            }
    }
    if (out->slot_animation[0] >= 0) out->idle_animation = out->slot_animation[0];
    // the eyes' texture animations (header 0x18 sleep, 0x1C wake, 0x20 blink; s32, -1 = none; verified: the loader
    // copies them to the sequence owner, sequence.c, and only texture-animation calls read them)
    for (int k = 0; k < 3; ++k) {
        const int v = (int)be32(pkx, 0x18 + 4 * (size_t)k);
        out->eye_texanim[k] = v >= 0 && v < out->animation_count ? v : -1;
    }
    // The shiny colouring (2026-11-19; snatcherLeo: "ensure all shinies are added for all Pokémon"): a model created as shiny
    // gets its PKX's colour block, when header flags bit 31 says there is one (378 of 527 files), right after the slot
    // records: 4 u32 channel indices, then 4 bytes of RGBA modulation (models-animation.md "PKX file"). The game
    // (wazaSequenceOwnerApplyShinyColors -> GSmodelEnableColorSwap / GSmodelEnableModulation, textures.md "Runtime
    // recolour") uses the swap only when the indices differ from 0, 1, 2, 3 and the modulation only when a byte
    // differs from 0x7F (verified from the code). Read from whichever PKX the shiny draws: the 35 separate rare_
    // models too, where they carry one
    // *Corrected 2026-11-19 (same day):* the block's place and byte order, from the game's loader (src/game/sequence.c,
    // the resource reader that fills sequence +0x38..+0x4D; verified from the code): with load mode 4 or more (5 on the
    // USA disc) the slot records are padded to 32 bytes first; the modulation is one u32 whose low byte is red (bits
    // 8-15 green, 16-23 blue, 24-31 alpha), so the file's four bytes run A, B, G, R; and when applying it the game sets
    // alpha to 0xFF (wazaSequenceOwnerApplyShinyColors), so alpha is never changed. The first try read the bytes in
    // file order right after the slots and drew every shiny black.
    if (shiny && (be32(pkx, 0x14) & 0x80000000u)) {
        const size_t c = o + (((size_t)slots * 0xD0 + 0x1F) & ~(size_t)0x1F);
        if (c + 20 <= pkx.size()) {
            bool swap = false, mod = false;
            for (int k = 0; k < 4; ++k) {
                const uint32_t ch = be32(pkx, c + 4 * (size_t)k);
                out->shiny_swap[k] = (uint8_t)std::min<uint32_t>(ch, 3);
                swap = swap || ch != (uint32_t)k;
            }
            const uint32_t m = be32(pkx, c + 16);
            for (int k = 0; k < 4; ++k) {
                out->shiny_mod[k] = (uint8_t)(m >> (8 * k));  // R, G, B, A
                mod = mod || out->shiny_mod[k] != 0x7F;
            }
            out->shiny_mod[3] = 0xFF;  // (the game's own override before applying)
            out->shiny_swap_on = swap ? 1 : 0;
            out->shiny_mod_on = mod ? 1 : 0;
        }
    }
    return 1;
}

extern "C" int orre_model_slot_animation(const OrreModel* m, int slot) {
    if (!m || slot < 0 || slot >= 17) return -1;
    // a slot the model doesn't have falls back as the game's does (wzx.md: 0x0B -> 0, 0x0C-0x0F -> 1)
    if (slot >= m->slot_count) {
        if (slot == 11) slot = 0;
        else if (slot >= 12 && slot <= 15) slot = 1;
        else return -1;
        if (slot >= m->slot_count) return -1;
    }
    return m->slot_animation[slot];
}

// ---- moves (the Moves tab, 2026-11-19; layouts in orre_core.h, from docs/engine/pokemon-data.md and
// models-animation.md, and the extractor's moves_decoded.csv / world.move_effects, which the golden tests compare) ----
namespace {
constexpr uint32_t kMovesOff = 0x11C7C4, kMovesCount = 0x14A6D4, kMoveSize = 0x38;
constexpr uint32_t kMachineTable = 0x80368018, kMachineCount = 58;
constexpr uint32_t kMoveEffects = 0x8036E150, kMoveEffectsSize = 0x26F0, kMoveEffectSize = 0x1C;
constexpr uint32_t kOwnEffects = 0x80373750, kOwnEffectsSize = 0x16F0, kOwnEffectSize = 0x10;
}  // namespace

extern "C" int orre_move_count(OrreDisc* od) {
    if (!od) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    return (int)table_rows(od->d.rel(), kMovesOff, kMovesCount, kMoveSize).size();
}

extern "C" int orre_move_get(OrreDisc* od, int move, OrreMove* out) {
    if (!od || !out || move < 0) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    auto rows = table_rows(od->d.rel(), kMovesOff, kMovesCount, kMoveSize);
    if (move >= (int)rows.size()) return 0;
    const uint8_t* e = rows[(size_t)move];
    *out = OrreMove{};
    out->index = move;
    out->priority = (int8_t)e[0];
    out->pp = e[1];
    out->type = e[2];
    out->target = e[3];
    out->accuracy = e[4];
    out->effect_chance = e[5];
    out->contact = e[6];
    out->power = (int16_t)be16(e + 0x16);
    out->effect = be16(e + 0x1A);
    out->name_msg = be32(e + 0x20);
    out->desc_msg = be32(e + 0x2C);
    out->wzx_id = be32(e + 0x30);
    return 1;
}

extern "C" int orre_machine_move(OrreDisc* od, int machine) {
    if (!od || machine < 0 || machine >= (int)kMachineCount) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    Bytes t;
    if (!od->d.dol_read(kMachineTable + 8 * (uint32_t)machine + 4, 4, t)) return 0;
    return (int)be32(t, 0);
}

extern "C" int orre_move_effect(OrreDisc* od, int move, uint32_t model_id, OrreMoveEffect* out) {
    if (!od || !out || move <= 0) return 0;
    *out = OrreMoveEffect{};
    out->slot = -1;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    uint32_t archive_id = 0, entry_id = 0;
    Bytes t;
    // which part the user plays: part 1 (the user's half), or part 3 for the moves the user does to itself (Growth,
    // Synthesis, Protect, Rest, Double Team...), whose general record has no part 1 (verified on main.dol 2026-11-19:
    // Growth's record is {2074, 0, 0, 0, 0, archive, entry})
    int part = 1;
    uint32_t gen_arc = 0, gen_ent = 0;
    if ((uint32_t)move < kMoveEffectsSize / kMoveEffectSize &&
        od->d.dol_read(kMoveEffects + (uint32_t)move * kMoveEffectSize, kMoveEffectSize, t)) {
        gen_arc = be32(t, 4);
        gen_ent = be32(t, 8);
        if (!gen_arc) {
            part = 3;
            gen_arc = be32(t, 20);
            gen_ent = be32(t, 24);
        }
    }
    // the Pokémon model's own version first, as wazaSequenceGetWazaResource looks (variant 0: the plain one)
    // *Corrected 2026-11-19 (release step 4):* only that part's records (part 1: 1..0x11B; part 3: 0x12C..0x15D), and
    // the first record for the model and move ends the search, used only when it names a file (verified from the
    // code); the whole table was searched before, which could take another part's record
    const uint32_t first = part == 1 ? 1 : 0x12C, last = part == 1 ? 0x11C : 0x15E;
    for (uint32_t i = first; i < last && i < kOwnEffectsSize / kOwnEffectSize; ++i) {
        if (!od->d.dol_read(kOwnEffects + i * kOwnEffectSize, kOwnEffectSize, t)) break;
        if (be16(t, 2) == model_id && be16(t, 4) == (uint32_t)move && t[0] == 0) {
            if (be32(t, 8) && be32(t, 12)) {
                archive_id = be32(t, 8);
                entry_id = be32(t, 12);
                out->own_version = 1;
            }
            break;
        }
    }
    // otherwise the general one
    if (!archive_id) {
        archive_id = gen_arc;
        entry_id = gen_ent;
    }
    if (!archive_id) return 0;
    od->d.load_toc();
    auto it = od->d.toc.find(archive_id);
    if (it == od->d.toc.end()) return 0;
    std::snprintf(out->archive, sizeof out->archive, "%s", it->second.c_str());
    out->entry_id = entry_id;
    // the WZX sequence header after the 0x70-byte node header: +0x00 the user's animation slot
    Bytes wzx;
    if (find_entry(od->d, archive_id, entry_id, wzx) && wzx.size() >= 0x74) {
        const int slot = (int)be32(wzx, 0x70);
        if (slot >= 0 && slot < 17) out->slot = slot;
    }
    return 1;
}
