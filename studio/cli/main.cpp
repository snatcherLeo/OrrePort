// orrestudio-cli: the reader core's command-line mode (the Studio review's decision 6: "a command-line mode for the
// core from day one"). Why: it is the core's test harness (the golden tests drive it), a quick way to look at a record
// without the window, and later the modders' batch tool (check a mod, pack an .orremod). Plain console program over the
// C API; the same on every OS.
//
//   orrestudio-cli --disc <image> info
//   orrestudio-cli --disc <image> dump species <index|all>        one JSON object per species
//   orrestudio-cli --disc <image> dump text <message id>
//   orrestudio-cli --disc <image> dump mugshot <index> <normal|shiny> <0|1 facing> <out.png>
//   orrestudio-cli --disc <image> dump cry <index> <out.wav>         the species' cry (cry.cpp); "all": one line each
//   orrestudio-cli --disc <image> dump pc <index> <normal|shiny> <out.png>
//   orrestudio-cli --disc <image> dump model <index> <normal|shiny> <out.txt>   every triangle vertex, 4 decimals
//
// The disc can also come from the ORRE_DISC environment variable. Output that comes from the disc (names, pictures,
// numbers) is the player's own data: it is printed or written where asked, never into the repo.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>
#include <string>
#include <vector>

#include "orre_core.h"

namespace {
void json_str(const char* s) {
    std::putchar('"');
    for (; *s; ++s) {
        const unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') std::printf("\\%c", c);
        else if (c < 0x20) std::printf("\\u%04x", c);
        else std::putchar(c);
    }
    std::putchar('"');
}

void dump_species(OrreDisc* d, int i) {
    OrreSpecies s;
    if (!orre_species_get(d, i, &s)) return;
    std::printf("{\"index\": %d, \"name\": ", i);
    json_str(orre_text(d, s.name_msg));
    std::printf(", \"nationalDex\": %u, \"hoennFirst\": %u, \"pkxDataId\": %u, \"types\": [%u, %u], \"abilities\": [%u, %u], ",
                s.national_dex, s.hoenn_first, s.model_id, s.types[0], s.types[1], s.abilities[0], s.abilities[1]);
    std::printf("\"baseStats\": [%u, %u, %u, %u, %u, %u], ", s.base_stats[0], s.base_stats[1], s.base_stats[2], s.base_stats[3],
                s.base_stats[4], s.base_stats[5]);
    std::printf("\"faces\": [[%u, %u, %u], [%u, %u, %u]], ", s.faces[0].colour, s.faces[0].mug_shot_id, s.faces[0].pc_picture_id,
                s.faces[1].colour, s.faces[1].mug_shot_id, s.faces[1].pc_picture_id);
    std::printf("\"category\": ");
    json_str(orre_text(d, s.category_msg));
    std::printf(", \"growth\": %u, \"catchRate\": %u, \"genderRatio\": %u, \"expYield\": %u, \"friendship\": %u, \"height\": %u, "
                "\"weight\": %u, \"cry\": %u",
                s.growth_rate, s.catch_rate, s.gender_ratio, s.exp_yield, s.base_friendship, s.height, s.weight, s.cry);
    // the Moves tab's lists (2026-11-19): TM/HM flags, egg moves, level-up {level, move}
    std::printf(", \"machines\": [");
    for (int k = 0; k < 58; ++k) std::printf(k ? ", %u" : "%u", s.machines[k]);
    std::printf("], \"eggMoves\": [");
    for (int k = 0; k < 8; ++k) std::printf(k ? ", %u" : "%u", s.egg_moves[k]);
    std::printf("], \"levelMoves\": [");
    bool first = true;
    for (const auto& lm : s.level_moves) {
        if (!lm.move) continue;
        std::printf(first ? "[%u, %u]" : ", [%u, %u]", lm.level, lm.move);
        first = false;
    }
    std::printf("]}\n");
}

int usage() {
    std::fprintf(stderr,
                 "usage: orrestudio-cli [--disc <image>] info\n"
                 "       orrestudio-cli [--disc <image>] dump species <index|all>\n"
                 "       orrestudio-cli [--disc <image>] dump moves all | dump machines all\n"
                 "       orrestudio-cli [--disc <image>] learnset <index>\n"
                 "       orrestudio-cli [--disc <image>] dump text <message id>\n"
                 "       orrestudio-cli [--disc <image>] dump mugshot <index> <normal|shiny> <facing 0|1> <out.png>\n"
                 "       orrestudio-cli [--disc <image>] dump cry <index|all> [out.wav]\n"
                 "       orrestudio-cli [--disc <image>] dump pc <index> <normal|shiny> <out.png>\n"
                 "       orrestudio-cli [--disc <image>] dump model <index> <normal|shiny> <out.txt>\n");
    return 2;
}

bool shiny_arg(const char* s) { return std::strcmp(s, "shiny") == 0 || std::strcmp(s, "1") == 0; }

// survey: one tab-separated line per species (and per extra form) listing what its battle model uses, so viewer
// fixes can be ranked by how many species they help (snatcherLeo's 2026-11-18 model review). Columns:
//   species, form, name, meshes, xlu (translucent meshes), pe (meshes with their own pixel-engine settings),
//   add (pe blending that adds: src or dst factor One), coord1..coord6 (stages by coordinate source: reflection,
//   highlight, shadow, toon, gradation, backlight), spec / amb / shad (stages on the specular, ambient, shadow
//   channel), tev, billboard, ptcl_joints, colour_anim, render_anim, hidden (meshes hidden at rest), gpt1 (bytes),
//   dropped, maxset, bump (emboss bump-map stages) and bumpmesh (meshes with one).
void survey(OrreDisc* d) {
    std::printf("species\tform\tname\tmeshes\txlu\tpe\tadd\tcoord1\tcoord2\tcoord3\tcoord4\tcoord5\tcoord6\tspec\tamb\tshad\t"
                "tev\tbillboard\tptcl_joints\tcolour_anim\trender_anim\thidden\tgpt1\tdropped\tmaxset\tbump\tbumpmesh\n");
    for (int sp = 1; sp < orre_species_count(d); ++sp) {
        OrreSpecies s;
        if (!orre_species_get(d, sp, &s)) continue;
        const int forms = orre_species_form_count(d, sp);
        for (int form = 0; form < forms; ++form) {
            OrreModel m{};
            if (!orre_species_form_model(d, sp, form, 0, &m)) continue;
            // bump / bumpmesh (2026-11-19, snatcherLeo: which Pokémon use the emboss bump maps): stages flagged TEX_BUMP (bit
            // 24, include/hsd/hsd_tobj.h), and the meshes that have one (the game keeps one bump stage per material)
            int xlu = 0, pe = 0, add = 0, coord[8] = {}, spec = 0, amb = 0, shad = 0, hidden = 0, bump = 0, bumpmesh = 0;
            for (int i = 0; i < m.mesh_count; ++i) {
                const OrreMesh& me = m.meshes[i];
                xlu += (me.render_mode >> 30) & 1;
                hidden += me.hidden;
                bool has_bump = false;
                for (int k = 0; k < me.stage_count; ++k)
                    if ((me.stages[k].flags >> 24) & 1) { ++bump; has_bump = true; }
                bumpmesh += has_bump ? 1 : 0;
                if (me.has_pe) {
                    ++pe;
                    if (me.pe[4] == 1 && (me.pe[5] == 1 || me.pe[6] == 1)) ++add;
                }
                for (int k = 0; k < me.stage_count; ++k) {
                    const uint32_t f = me.stages[k].flags;
                    ++coord[f & 7];
                    spec += (f >> 5) & 1;
                    amb += (f >> 6) & 1;
                    shad += (f >> 8) & 1;
                }
            }
            OrreModelSurvey v{};
            orre_model_survey(&m, &v);
            std::printf("%d\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", sp,
                        form, orre_text(d, s.name_msg), m.mesh_count, xlu, pe, add, coord[1], coord[2], coord[3], coord[4],
                        coord[5], coord[6], spec, amb, shad, v.tev_stages, v.billboard_joints, v.particle_joints, v.colour_anims,
                        v.render_anims, hidden, m.particle_bank_size, v.dropped_stages, v.max_uv_set, bump, bumpmesh);
            orre_model_free(&m);
        }
    }
}
}  // namespace

int main(int argc, char** argv) {
    const char* disc_path = std::getenv("ORRE_DISC");
    int a = 1;
    if (a + 1 < argc && std::strcmp(argv[a], "--disc") == 0) {
        disc_path = argv[a + 1];
        a += 2;
    }
    // --files <image>: models read from another region's disc, found through --disc's tables (texlog; a region's own
    // style, 2026-11-22: Jynx's Japanese model)
    const char* files_path = nullptr;
    if (a + 1 < argc && std::strcmp(argv[a], "--files") == 0) {
        files_path = argv[a + 1];
        a += 2;
    }
    if (a >= argc) return usage();
    char err[512] = "";
    OrreDisc* d = orre_disc_open(disc_path, err, sizeof err);
    if (!d) {
        std::fprintf(stderr, "orrestudio-cli: %s\n", err);
        return 1;
    }
    int rc = 0;
    const std::string cmd = argv[a];
    if (cmd == "info") {
        char sha[41] = "";
        orre_disc_main_dol_sha1(d, sha);  // (the disc's program file's fingerprint: which release and revision, 2026-11-23)
        std::printf("game id: %s\nmain.dol sha1: %s\nspecies: %d\n", orre_disc_game_id(d), sha[0] ? sha : "none", orre_species_count(d));
    } else if (cmd == "survey") {
        survey(d);
    } else if (cmd == "bindcheck" && a + 1 < argc) {
        // bindcheck <species>: posing with no animation must give back the stored (bind-pose) vertices; prints each
        // mesh's largest difference (a check on the posing code: skinning, IK, constraints)
        OrreModel m{};
        if (orre_species_model(d, std::atoi(argv[a + 1]), 0, &m)) {
            std::vector<std::vector<OrreVertex>> buf((size_t)m.mesh_count);
            std::vector<OrreVertex*> out((size_t)m.mesh_count);
            for (int i = 0; i < m.mesh_count; ++i) { buf[(size_t)i].resize((size_t)m.meshes[i].vertex_count); out[(size_t)i] = buf[(size_t)i].data(); }
            orre_model_pose(&m, -1, 0, out.data());
            for (int i = 0; i < m.mesh_count; ++i) {
                double worst = 0;
                for (int v = 0; v < m.meshes[i].vertex_count; ++v)
                    for (int k = 0; k < 3; ++k)
                        worst = std::max(worst, (double)std::fabs(buf[(size_t)i][(size_t)v].pos[k] - m.meshes[i].vertices[v].pos[k]));
                if (worst > 1e-3) std::printf("mesh %d off by %.4f (hidden %d)\n", i, worst, m.meshes[i].hidden);
            }
            std::printf("checked %d meshes\n", m.mesh_count);
        }
        orre_model_free(&m);
    } else if (cmd == "inventory" && a + 2 < argc && std::strcmp(argv[a + 1], "moves") == 0) {
        // inventory moves <out.md> (snatcherLeo, 2026-11-19: "Start an inventory list ... for what moves are physical, special,
        // status in Gen 3/Colosseum/XD - and as well, the Type for each move, that'll eventually be clickable into the
        // "Type Editor" for that specific Type"): every move on the player's disc grouped by kind, with its type. Written
        // from the disc each time (a generated listing: it goes to a git-ignored folder, never into the repo). The kind
        // follows Gen 3's rule: status when the move has no power, else its type's category byte (the type table,
        // battle-types.md, verified); XD's move values are the same as Colosseum's (verified 2026-11-19 by the project).
        static const char* kType[18] = {"Normal", "Fighting", "Flying", "Poison", "Ground", "Rock", "Bug", "Ghost", "Steel",
                                        "???", "Fire", "Water", "Grass", "Electric", "Psychic", "Ice", "Dragon", "Dark"};
        auto title = [](std::string s) {  // the disc stores names in capitals: "VINE WHIP" -> "Vine Whip"
            bool start = true;
            for (char& c : s) {
                if (std::isalpha((unsigned char)c)) { c = start ? (char)std::toupper((unsigned char)c) : (char)std::tolower((unsigned char)c); start = false; }
                else start = c == ' ' || c == '-';
            }
            return s;
        };
        struct Row { int id; std::string name; OrreMove mv; };
        std::vector<Row> kinds[3];  // physical, special, status
        for (int i = 1; i < orre_move_count(d); ++i) {
            OrreMove mv{};
            if (!orre_move_get(d, i, &mv)) break;
            const std::string nm = orre_text(d, mv.name_msg);
            if (nm.empty() || nm == "????") continue;  // (the last record is an unused placeholder)
            const int c = mv.power == 0 ? 2 : orre_type_category(d, mv.type) == 2 ? 1 : 0;
            kinds[c].push_back({i, title(nm), mv});
        }
        std::FILE* f = std::fopen(argv[a + 2], "wb");
        if (!f) { std::fprintf(stderr, "orrestudio-cli: can't write %s\n", argv[a + 2]); return 1; }
        std::fprintf(f, "# Moves by kind: Gen 3 / Colosseum / XD\n\nGenerated from the player's own disc by `orrestudio-cli inventory "
                        "moves` (not part of the repo). Kind: Gen 3 decides physical or special by the move's **type** (the type "
                        "table below); a move with no power is **status**. Type names will link to the Type editor in Studio.\n\n");
        std::fprintf(f, "Totals: %zu physical, %zu special, %zu status.\n\n## Types and their kind\n\n| Type | Kind |\n|---|---|\n",
                     kinds[0].size(), kinds[1].size(), kinds[2].size());
        for (int t = 0; t < 18; ++t) {
            const int c = orre_type_category(d, t);
            std::fprintf(f, "| %s | %s |\n", kType[t], c == 1 ? "physical" : c == 2 ? "special" : "neither");
        }
        static const char* kHead[3] = {"Physical", "Special", "Status"};
        for (int k = 0; k < 3; ++k) {
            auto& v = kinds[k];
            std::stable_sort(v.begin(), v.end(), [](const Row& x, const Row& y) {
                return x.mv.type != y.mv.type ? x.mv.type < y.mv.type : x.name < y.name;
            });
            std::fprintf(f, "\n## %s (%zu)\n\n| Type | Move | No. | Power | Acc. | PP |\n|---|---|---|---|---|---|\n", kHead[k], v.size());
            for (const Row& r : v)
                std::fprintf(f, "| %s | %s | %d | %s | %s | %d |\n", r.mv.type < 18 ? kType[r.mv.type] : "?", r.name.c_str(), r.id,
                             r.mv.power < 2 ? "—" : std::to_string(r.mv.power).c_str(),
                             r.mv.accuracy == 0 ? "—" : std::to_string(r.mv.accuracy).c_str(), r.mv.pp);
        }
        std::fclose(f);
        std::printf("%zu physical, %zu special, %zu status -> %s\n", kinds[0].size(), kinds[1].size(), kinds[2].size(), argv[a + 2]);
    } else if (cmd == "ptclsim" && a + 2 < argc) {
        // ptclsim <species> <frames> [seed] [animation]: plays the animation (default the idle) with the model's own
        // particles as the battle does each 30 fps frame (keys fire, finished emitters are reclaimed, generators move
        // to their joints, then 2 steps of 60 Hz) and prints the live particles every 10 frames. Docs reference
        // (models-animation.md, the PC port running the game's code): after 60 frames of animation 0, Charizard 180,
        // Moltres 107, Magmar 104, Charmeleon 96, Charmander 65, Weezing 45, Gastly 40, Articuno 21, Koffing 9, Vaporeon 2
        OrreModel m{};
        if (!orre_species_model(d, std::atoi(argv[a + 1]), 0, &m) || !m.particles) {
            std::printf("no particle bank\n");
            orre_model_free(&m);
            return 0;
        }
        const int frames = std::atoi(argv[a + 2]);
        const uint32_t seed = a + 3 < argc ? (uint32_t)std::strtoul(argv[a + 3], nullptr, 0) : 1u;
        const int anim = a + 4 < argc ? std::atoi(argv[a + 4]) : (m.idle_animation >= 0 ? m.idle_animation : 0);
        std::vector<OrreParticleKey> keys(512);
        const int nk = std::min(512, orre_model_particle_keys(&m, anim, keys.data(), 512));
        const int len = std::max(1, (int)orre_model_animation_length(&m, anim));
        OrreParticleSystem* ps = orre_particles_create();
        orre_particles_reset(ps, seed);
        orre_particles_set_bank(ps, 0, m.particles);
        struct Live { int handle, joint; };
        std::vector<Live> live;
        std::vector<float> w((size_t)m.joint_count * 12), sc((size_t)m.joint_count * 3);
        for (int f = 0; f < frames; ++f) {
            const int af = f % len;
            for (int k = 0; k < nk; ++k)
                if (keys[(size_t)k].frame == af) {
                    const int h = orre_particles_start(ps, 0, keys[(size_t)k].emitter);
                    if (h) live.push_back({h, keys[(size_t)k].joint});
                }
            orre_particles_reclaim(ps);
            orre_model_joint_world(&m, anim, (float)af, w.data(), sc.data());
            for (const Live& l : live) {
                const float p[3] = {w[(size_t)l.joint * 12 + 3], w[(size_t)l.joint * 12 + 7], w[(size_t)l.joint * 12 + 11]};
                orre_particles_set_position(ps, l.handle, p);
            }
            orre_particles_step(ps);
            orre_particles_step(ps);
            if ((f + 1) % 10 == 0)
                std::printf("frame %d: %d particles, %d generators\n", f + 1, orre_particles_count(ps), orre_particles_generators(ps));
        }
        std::printf("animation %d (%d frames), %d keys\n", anim, len, nk);
        orre_particles_free(ps);
        orre_model_free(&m);
    } else if (cmd == "learnset" && a + 1 < argc) {
        // learnset <species>: the Moves tab's data (2026-11-19): level-up, TM/HM and egg moves with each move's record,
        // its effect archive, whether this Pokémon has its own version, the user's animation slot and the animation it
        // plays on this model (checked against the extractor's moves_decoded.csv and effects/ README in the golden tests)
        const int sp = std::atoi(argv[a + 1]);
        OrreSpecies s{};
        if (!orre_species_get(d, sp, &s)) { std::fprintf(stderr, "orrestudio-cli: no species %d\n", sp); return 1; }
        OrreModel m{};
        const bool has_model = orre_species_model(d, sp, 0, &m) != 0;
        auto line = [&](const char* how, int move) {
            OrreMove mv{};
            if (!orre_move_get(d, move, &mv)) return;
            OrreMoveEffect fx{};
            const bool has_fx = orre_move_effect(d, move, s.model_id, &fx) != 0;
            const int kind = mv.power == 0 ? 0 : orre_type_category(d, mv.type);
            std::printf("%-6s %3d %-14s type %2d %-8s power %3d acc %3d pp %2d prio %+d | %s%s slot %d anim %d\n", how, move,
                        orre_text(d, mv.name_msg), mv.type, kind == 1 ? "physical" : kind == 2 ? "special" : "status", mv.power,
                        mv.accuracy, mv.pp, mv.priority, has_fx ? fx.archive : "-", fx.own_version ? " (own)" : "", fx.slot,
                        has_model ? orre_model_slot_animation(&m, fx.slot) : -1);
        };
        std::printf("%s (model %u, %d slots)\n", orre_text(d, s.name_msg), s.model_id, has_model ? m.slot_count : 0);
        for (const auto& lm : s.level_moves) {
            if (!lm.move) continue;
            char how[8];
            std::snprintf(how, sizeof how, "L%d", lm.level);
            line(how, lm.move);
        }
        for (int k = 0; k < 58; ++k) {
            if (!s.machines[k]) continue;
            char how[8];
            std::snprintf(how, sizeof how, k < 50 ? "TM%02d" : "HM%02d", k < 50 ? k + 1 : k - 49);
            line(how, orre_machine_move(d, k));
        }
        for (uint16_t e : s.egg_moves)
            if (e) line("egg", e);
        if (has_model) orre_model_free(&m);
    } else if (cmd == "jointworld" && a + 3 < argc) {
        // jointworld <species> <animation|idle> <frame> [joint,joint,...]: each joint's world matrix (3 rows of 4) and
        // accumulated scale as Studio poses it, in the layout of the move viewer's joint log, to compare the two
        // (Lunatone's and Sharpedo's jaws, 2026-11-19)
        OrreModel m{};
        if (orre_species_model(d, std::atoi(argv[a + 1]), 0, &m)) {
            const int anim = std::strcmp(argv[a + 2], "idle") == 0 ? m.idle_animation : std::atoi(argv[a + 2]);
            std::vector<float> w((size_t)m.joint_count * 12), s((size_t)m.joint_count * 3);
            orre_model_joint_world(&m, anim, std::strtof(argv[a + 3], nullptr), w.data(), s.data());
            std::string only = a + 4 < argc ? std::string(",") + argv[a + 4] + "," : "";
            std::printf("animation %d frame %s\n", anim, argv[a + 3]);
            for (int j = 0; j < m.joint_count; ++j) {
                if (!only.empty() && only.find("," + std::to_string(j) + ",") == std::string::npos) continue;
                std::printf("joint %d scl %.6f %.6f %.6f\n", j, s[(size_t)j * 3], s[(size_t)j * 3 + 1], s[(size_t)j * 3 + 2]);
                for (int r = 0; r < 3; ++r)
                    std::printf("  row %d: %.6f %.6f %.6f %.6f\n", r, w[(size_t)j * 12 + (size_t)r * 4], w[(size_t)j * 12 + (size_t)r * 4 + 1],
                                w[(size_t)j * 12 + (size_t)r * 4 + 2], w[(size_t)j * 12 + (size_t)r * 4 + 3]);
            }
        }
        orre_model_free(&m);
    } else if (cmd == "posebox" && a + 2 < argc) {
        // posebox <species> <frame>: each part's box in the bind pose and posed on that idle frame (where an animation
        // moves a part; written to look into Lunatone's mouth against the game's move viewer, 2026-11-19)
        OrreModel m{};
        if (orre_species_model(d, std::atoi(argv[a + 1]), 0, &m)) {
            std::vector<std::vector<OrreVertex>> buf((size_t)m.mesh_count);
            std::vector<OrreVertex*> out((size_t)m.mesh_count);
            for (int i = 0; i < m.mesh_count; ++i) { buf[(size_t)i].resize((size_t)m.meshes[i].vertex_count); out[(size_t)i] = buf[(size_t)i].data(); }
            orre_model_pose(&m, m.idle_animation, std::strtof(argv[a + 2], nullptr), out.data());
            auto box = [](const OrreVertex* v, int n) {
                float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
                for (int k = 0; k < n; ++k)
                    for (int c = 0; c < 3; ++c) { lo[c] = std::min(lo[c], v[k].pos[c]); hi[c] = std::max(hi[c], v[k].pos[c]); }
                std::printf("x %6.2f..%6.2f y %6.2f..%6.2f z %6.2f..%6.2f", lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]);
            };
            for (int i = 0; i < m.mesh_count; ++i) {
                std::printf("mesh %2d bind  ", i);
                box(m.meshes[i].vertices, m.meshes[i].vertex_count);
                std::printf("\n        posed ");
                box(buf[(size_t)i].data(), m.meshes[i].vertex_count);
                std::printf("\n");
            }
        }
        orre_model_free(&m);
    } else if (cmd == "texlog" && a + 1 < argc) {
        // texlog <species> [shiny]: what Studio draws for every part, one line per polygon object, keyed as the game's
        // ORRE_WAZA_VIEWER_TEXLOG lines are (part, display object, polygon object), with the material and, per stage, the
        // image and palette (offsets into the archive's data block), format, size, wrap, flags and TEV override. The
        // all-models check (2026-11-19) compares the two lists, so every texture and material is checked exactly
        OrreModel m{};
        OrreDisc* files = files_path ? orre_disc_open(files_path, err, sizeof err) : nullptr;
        if (files_path && !files) std::fprintf(stderr, "orrestudio-cli: --files: %s\n", err);
        if (orre_species_form_model_from(d, files ? files : d, std::atoi(argv[a + 1]), 0, a + 2 < argc && shiny_arg(argv[a + 2]), &m)) {
            // the shiny recolour the model was read with (none unless read as shiny with a colour block)
            std::printf("shiny swap %s %d %d %d %d modulation %s %02X%02X%02X%02X\n", m.shiny_swap_on ? "on" : "off", m.shiny_swap[0],
                        m.shiny_swap[1], m.shiny_swap[2], m.shiny_swap[3], m.shiny_mod_on ? "on" : "off", m.shiny_mod[0], m.shiny_mod[1],
                        m.shiny_mod[2], m.shiny_mod[3]);
            int last_j = -1, last_d = -1, p = 0;
            for (int i = 0; i < m.mesh_count; ++i) {
                const OrreMesh& me = m.meshes[i];
                int j = -1, dobj = -1;
                orre_model_mesh_owner(&m, i, &j, &dobj);
                p = (j == last_j && dobj == last_d) ? p + 1 : 0;
                last_j = j; last_d = dobj;
                std::printf("part %d dobj %d pobj %d: render 0x%08X diffuse %02X%02X%02X%02X ambient %02X%02X%02X%02X specular "
                            "%02X%02X%02X%02X alpha %.4f shininess %.4f cull %d",
                            j, dobj, p, me.render_mode, me.diffuse[0], me.diffuse[1], me.diffuse[2], me.diffuse[3], me.ambient[0],
                            me.ambient[1], me.ambient[2], me.ambient[3], me.specular[0], me.specular[1], me.specular[2],
                            me.specular[3], me.alpha, me.shininess, me.cull);
                if (me.glow) std::printf(" glow");
                if (me.hidden) std::printf(" hidden");
                if (me.has_pe) {
                    std::printf(" pe");
                    for (int k = 0; k < 12; ++k) std::printf(" %d", me.pe[k]);
                } else std::printf(" pe none");
                std::printf(" stages %d\n", me.stage_count);
                for (int s = 0; s < me.stage_count; ++s) {
                    const OrreTexStage& st = me.stages[s];
                    OrreTextureSource src{};
                    orre_model_texture_source(&m, st.texture, &src);
                    std::printf("  stage %d: image 0x%X format %u %dx%d tlut 0x%X format %u wrap %d %d flags 0x%08X blend %.4f map %u "
                                "uv %d scale %.4f %.4f translate %.4f %.4f repeat %d %d tev",
                                s, src.data_offset, src.format, src.width, src.height, src.tlut_offset, src.tlut_format, st.wrap_s,
                                st.wrap_t, st.flags, st.blending, st.map_id, st.uv_set, st.scale[0], st.scale[1], st.translate[0],
                                st.translate[1], st.repeat[0], st.repeat[1]);
                    if (st.has_tev)
                        for (int k = 0; k < 32; ++k) std::printf(" %02X", st.tev[k]);
                    else std::printf(" none");
                    // the decoded picture's content (FNV-1a over all its RGBA), to tell a separate shiny model's pictures
                    // from the normal one's when their layout is the same
                    uint32_t h = 0x811C9DC5u;
                    if (st.texture >= 0 && st.texture < m.texture_count) {
                        const OrreImage& im = m.textures[st.texture];
                        for (size_t q = 0; q < (size_t)im.width * im.height * 4; ++q) h = (h ^ im.rgba[q]) * 0x01000193u;
                    }
                    std::printf(" pixels %08X\n", h);
                }
            }
        }
        orre_model_free(&m);
    } else if (cmd == "normallen") {
        // normallen: per species, the stored normals' lengths on lit parts (bind pose): mean, min, max. GX lights with
        // the normal as given (no renormalising on the hardware, likely), so normals longer than 1 light brighter
        for (int sp = 1; sp < orre_species_count(d); ++sp) {
            OrreModel m{};
            if (!orre_species_model(d, sp, 0, &m)) continue;
            double sum = 0, mn = 1e9, mx = 0;
            long n = 0;
            for (int i = 0; i < m.mesh_count; ++i) {
                const OrreMesh& me = m.meshes[i];
                if (!(me.render_mode & 4)) continue;
                for (int k = 0; k < me.vertex_count; ++k) {
                    const float* v = me.vertices[k].nrm;
                    const double l = std::sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
                    if (l < 1e-6) continue;
                    sum += l; mn = std::min(mn, l); mx = std::max(mx, l); ++n;
                }
            }
            if (n) std::printf("%d\t%.3f\t%.3f\t%.3f\n", sp, sum / n, mn, mx);
            orre_model_free(&m);
        }
    } else if (cmd == "normalcheck") {
        // normalcheck: every species and form, normal and shiny: the lit parts (RENDER_DIFFUSE) whose vertices have no
        // usable normal (zero length). Game lighting needs a normal at every lit vertex; a part without one goes near
        // black (Regirock and Groudon did, 2026-11-19, before GX_VA_NBT normals were kept). One line per bad part, then
        // the totals
        int models = 0, bad_parts = 0;
        for (int sp = 1; sp < orre_species_count(d); ++sp) {
            const int forms = orre_species_form_count(d, sp);
            for (int form = 0; form < forms; ++form)
                for (int shiny = 0; shiny < 2; ++shiny) {
                    OrreModel m{};
                    if (!orre_species_form_model(d, sp, form, shiny, &m)) continue;
                    ++models;
                    for (int i = 0; i < m.mesh_count; ++i) {
                        const OrreMesh& me = m.meshes[i];
                        if (!(me.render_mode & 4)) continue;  // unlit: no normal needed
                        int zero = 0;
                        for (int k = 0; k < me.vertex_count; ++k) {
                            const float* n = me.vertices[k].nrm;
                            if (n[0] * n[0] + n[1] * n[1] + n[2] * n[2] < 1e-8f) ++zero;
                        }
                        if (zero) {
                            ++bad_parts;
                            std::printf("species %d form %d %s mesh %d: %d of %d vertices without a normal\n", sp, form,
                                        shiny ? "shiny" : "normal", i, zero, me.vertex_count);
                        }
                    }
                    orre_model_free(&m);
                }
        }
        std::printf("%d models checked, %d lit parts with missing normals\n", models, bad_parts);
    } else if (cmd == "framing") {
        // framing: one line per species with a battle model: species, the height of its middle (y, model space) and
        // the radius around (0, y, 0) that holds every shown vertex on idle frame 0. The all-models check (2026-11-19,
        // snatcherLeo: "check and verify all of these models") aims the game's move viewer and Studio's game camera from this,
        // so each Pokémon fills the shot instead of the viewer's preset, where most are a few dozen pixels tall
        for (int sp = 1; sp < orre_species_count(d); ++sp) {
            OrreModel m{};
            if (!orre_species_model(d, sp, 0, &m)) continue;
            std::vector<std::vector<OrreVertex>> buf((size_t)m.mesh_count);
            std::vector<OrreVertex*> out((size_t)m.mesh_count);
            for (int i = 0; i < m.mesh_count; ++i) { buf[(size_t)i].resize((size_t)m.meshes[i].vertex_count); out[(size_t)i] = buf[(size_t)i].data(); }
            if (m.idle_animation >= 0) orre_model_pose(&m, m.idle_animation, 0, out.data());
            float lo = 1e9f, hi = -1e9f;
            for (int i = 0; i < m.mesh_count; ++i) {
                if (m.meshes[i].hidden) continue;
                const OrreVertex* v = m.idle_animation >= 0 ? buf[(size_t)i].data() : m.meshes[i].vertices;
                for (int k = 0; k < m.meshes[i].vertex_count; ++k) { lo = std::min(lo, v[k].pos[1]); hi = std::max(hi, v[k].pos[1]); }
            }
            const float cy = (lo + hi) / 2;
            float r2 = 0;
            for (int i = 0; i < m.mesh_count; ++i) {
                if (m.meshes[i].hidden) continue;
                const OrreVertex* v = m.idle_animation >= 0 ? buf[(size_t)i].data() : m.meshes[i].vertices;
                for (int k = 0; k < m.meshes[i].vertex_count; ++k) {
                    const float dx = v[k].pos[0], dy = v[k].pos[1] - cy, dz = v[k].pos[2];
                    r2 = std::max(r2, dx * dx + dy * dy + dz * dz);
                }
            }
            if (hi >= lo) std::printf("%d\t%.3f\t%.3f\n", sp, cy, std::sqrt(r2));
            orre_model_free(&m);
        }
    } else if (cmd == "evolutions") {
        // evolutions: every evolution slot on the disc with its group, trigger and parameter, then the counts and the
        // items each item-based group uses (snatcherLeo, 2026-11-18: keep track of the evolution kinds for later editing)
        static const char* kCat[] = {"none", "level", "friendship", "trade", "item", "stats", "personality", "extra", "beauty"};
        static const char* kTrig[] = {"-", "level-up", "trade", "item use"};
        int count[9] = {};
        std::map<std::string, int> items_used, items_traded;
        for (int i = 0; i < orre_species_count(d); ++i) {
            OrreSpecies s{};
            if (!orre_species_get(d, i, &s)) continue;
            for (const auto& ev : s.evolutions) {
                if (!ev.kind) continue;
                OrreSpecies t{};
                orre_species_get(d, ev.target, &t);
                const OrreEvoCategory c = orre_evo_category(ev.kind);
                std::string param = std::to_string(ev.param);
                if (orre_evo_param(ev.kind) == ORRE_EVO_PARAM_ITEM) {
                    param = orre_text(d, orre_item_name_msg(d, ev.param));
                    (ev.kind == 7 ? items_used : items_traded)[param]++;
                } else if (orre_evo_param(ev.kind) == ORRE_EVO_PARAM_LEVEL) {
                    param = "Lv " + param;
                } else if (orre_evo_param(ev.kind) == ORRE_EVO_PARAM_NONE) {
                    param = "-";
                }
                ++count[c];
                std::printf("%-12s -> %-12s kind %2d  %-11s %-8s %s%s\n", orre_text(d, s.name_msg), orre_text(d, t.name_msg),
                            ev.kind, kCat[c], kTrig[orre_evo_trigger(ev.kind)], param.c_str(),
                            orre_evo_works_in_colosseum(ev.kind) ? "" : "  (never in Colosseum)");
            }
        }
        std::printf("\ncounts:");
        for (int c = 1; c < 9; ++c) std::printf(" %s %d", kCat[c], count[c]);
        std::printf("\nitems used (kind 7):");
        for (auto& [n, k] : items_used) std::printf(" %s x%d,", n.c_str(), k);
        std::printf("\nitems held in a trade (kind 6):");
        for (auto& [n, k] : items_traded) std::printf(" %s x%d,", n.c_str(), k);
        std::printf("\n");
    } else if (cmd == "ptclkeys" && a + 1 < argc) {
        // ptclkeys <species>: which animations start particles, at which joints, which emitters and when (the plan for
        // Studio's particle playback; models-animation.md "Ambient particle effects")
        OrreModel m{};
        if (orre_species_model(d, std::atoi(argv[a + 1]), 0, &m)) {
            std::printf("bank %d bytes, animations %d, idle %d\n", m.particle_bank_size, m.animation_count, m.idle_animation);
            for (int an = 0; an < m.animation_count; ++an) {
                const int n = orre_model_particle_keys(&m, an, nullptr, 0);
                if (!n) continue;
                std::vector<OrreParticleKey> k((size_t)n);
                orre_model_particle_keys(&m, an, k.data(), n);
                std::printf("animation %d (length %g): %d keys\n", an, orre_model_animation_length(&m, an), n);
                // one line per joint / bank / emitter: how often and the frames it fires on (first few)
                std::vector<bool> done((size_t)n);
                for (int i = 0; i < n; ++i) {
                    if (done[(size_t)i]) continue;
                    std::string frames;
                    int c = 0;
                    for (int j = i; j < n; ++j)
                        if (!done[(size_t)j] && k[j].joint == k[i].joint && k[j].bank == k[i].bank && k[j].emitter == k[i].emitter) {
                            done[(size_t)j] = true;
                            if (++c <= 8) frames += (frames.empty() ? "" : ",") + std::to_string(k[j].frame);
                        }
                    std::printf("  joint %d bank %d emitter %d (0x%X): %d times, frames %s%s\n", k[i].joint, k[i].bank,
                                k[i].emitter, k[i].emitter, c, frames.c_str(), c > 8 ? ",..." : "");
                }
            }
        }
        orre_model_free(&m);
    } else if (cmd == "wzx" && a + 2 < argc) {
        // wzx <move> <species> [part]: a move's effect file on a Pokémon (release step 4): the header, then every
        // entry in run order with its start (60 Hz ticks, against the species' time points for the file's slot)
        const int move = std::atoi(argv[a + 1]), sp = std::atoi(argv[a + 2]), part = a + 3 < argc ? std::atoi(argv[a + 3]) : 1;
        OrreSpecies s{};
        OrreModel m{};
        orre_species_get(d, sp, &s);
        orre_species_model(d, sp, 0, &m);
        // (a name instead of a move number: that sequence file, e.g. wzx_monsterball_downin)
        OrreWzx* w = move > 0 ? orre_move_wzx(d, move, s.model_id, part) : orre_wzx_named(d, argv[a + 1]);
        if (!w) {
            std::printf("no effect file for move %d part %d\n", move, part);
        } else {
            OrreWzxHeader h{};
            orre_wzx_header(w, &h);
            std::printf("slot %d flags 0x%X value %d mode %d camera %d bytes, %d entries; size class %d, effect scale %g\n",
                        h.slot, h.flags, h.value, h.mode, h.camera_size, h.count, m.size_class,
                        orre_wzx_effect_scale(m.size_class, move, part));
            const int slot = h.slot >= 0 && h.slot < 17 ? h.slot : 0;
            std::vector<int> st((size_t)h.count), ord((size_t)h.count);
            orre_wzx_schedule(w, m.slot_times[slot], st.data(), ord.data());
            static const char* kTypes[] = {"?", "value", "model", "particles", "effect", "sound", "control"};
            for (int k = 0; k < h.count; ++k) {
                OrreWzxEntry e{};
                orre_wzx_entry(w, ord[(size_t)k], &e);
                std::printf("  #%-3d t=%-4d key %-3d %-9s attach %-2d follow %d flags 0x%X", ord[(size_t)k], st[(size_t)ord[(size_t)k]],
                            e.key, e.type >= 0 && e.type <= 6 ? kTypes[e.type] : "?", e.attach, e.follow, e.flags);
                if (e.type == 3) std::printf(" emitter %d mode 0x%X %s", e.emitter, e.mode, e.has_bank ? "bank" : "linked bank");
                if (e.type == 4) std::printf(" kind %d frames %d", e.effect_kind, e.effect_frames);
                if (e.type == 5) std::printf(" sound %d", e.sound);
                if (e.type == 6) std::printf(" control %d", e.control);
                // ORRE_DEBUG_WZX_LINKS=1 (developer switch, any OS): each entry's start rule (the linked entry's key,
                // which of its time points and which of its own; how Howl's late darkening was looked into, 2026-11-21)
                if (std::getenv("ORRE_DEBUG_WZX_LINKS"))
                    std::printf(" [link %d base %d own %d len %d; time points %d %d %d %d]", e.link_key, e.base_index, e.own_index,
                                e.length_index, e.timing[0], e.timing[1], e.timing[2], e.timing[3]);
                if (e.type == 1) std::printf(" value kind %d %d", e.value_kind, e.value);
                // (linked to a model entry, entry flags 1: the part of that model it attaches to, +0x64)
                if ((e.flags & 1) && e.link_key > 0 && (e.type == 2 || e.type == 3)) std::printf(" (linked model's part %d)", e.part);
                std::printf(" (joint %d)\n", e.attach >= 0 && e.attach < 16 ? m.slot_points[slot][e.attach] : -1);
                // a blur's mode and keys
                OrreWzxBlur bl{};
                if (orre_wzx_blur(w, ord[(size_t)k], &bl)) {
                    std::printf("        blur %s:", bl.screen ? "screen" : "after-images");
                    for (int q = 0; q < bl.count; ++q) std::printf(" [%g->%g %d]", bl.key[q].from, bl.key[q].to, bl.key[q].ticks);
                    std::printf("\n");
                }
                // the env-map shine's settings and keys (screen effect kind 8)
                OrreWzxEnvMap em{};
                if (orre_wzx_envmap(w, ord[(size_t)k], &em)) {
                    std::printf("        env-map mode %d type %d:", em.mode, em.type);
                    for (int q = 0; q < em.count; ++q) std::printf(" [%g->%g %d]", em.key[q].from, em.key[q].to, em.key[q].ticks);
                    std::printf("\n");
                }
                // the screen warp's settings (screen effect kind 12)
                OrreWzxWarp wp{};
                if (orre_wzx_warp(w, ord[(size_t)k], &wp)) {
                    std::printf("        warp size %g strength %g peak %g particles %d follow %d, %d ticks, picture %dx%d%s\n", wp.size,
                                wp.strength, wp.peak, wp.particles, wp.follow, wp.frames, wp.tex_width, wp.tex_height,
                                wp.tex_rgba ? "" : " (not decoded)");
                    // the picture's channels (min / mean / max, the corner and the centre), to see what a neutral pixel is
                    if (wp.tex_rgba && wp.tex_width > 0 && wp.tex_height > 0) {
                        const int n = wp.tex_width * wp.tex_height;
                        for (int c = 0; c < 4; ++c) {
                            int lo = 255, hi = 0;
                            long long sum = 0;
                            for (int i = 0; i < n; ++i) {
                                const int v = wp.tex_rgba[4 * i + c];
                                lo = std::min(lo, v);
                                hi = std::max(hi, v);
                                sum += v;
                            }
                            const int mid = 4 * ((wp.tex_height / 2) * wp.tex_width + wp.tex_width / 2);
                            std::printf("          %c: %d..%d mean %lld, corner %d, centre %d\n", "RGBA"[c], lo, hi, sum / n,
                                        (int)wp.tex_rgba[c], (int)wp.tex_rgba[mid + c]);
                        }
                    }
                }
                // the leaves' settings (screen effect kind 5)
                OrreWzxLeaves lv{};
                if (orre_wzx_leaves(w, ord[(size_t)k], &lv))
                    std::printf("        leaves %d, size %g+-%g, end %g+-%g (first x %g+-%g), control %g+-%g, lift %g, speed %g, "
                                "fade %d..%d ticks, attach %d\n",
                                lv.count, lv.scale_base, lv.scale_range, lv.near_base, lv.near_range, lv.first_base, lv.first_range,
                                lv.far_base, lv.far_range, lv.lift, lv.speed, lv.fade_start, lv.frames, lv.attach);
                // a colour fade's keys (what it tints, then each key's from -> to colour as R G B A and its ticks)
                OrreWzxFade fd{};
                if (orre_wzx_fade(w, ord[(size_t)k], &fd)) {
                    std::printf("        fade target %d alpha %d ignore-rgb %d:", fd.target, fd.use_alpha, fd.ignore_rgb);
                    for (int q = 0; q < fd.count; ++q)
                        std::printf(" [%02X%02X%02X%02X->%02X%02X%02X%02X %d]", fd.key[q].from[0], fd.key[q].from[1], fd.key[q].from[2],
                                    fd.key[q].from[3], fd.key[q].to[0], fd.key[q].to[1], fd.key[q].to[2], fd.key[q].to[3], fd.key[q].len);
                    std::printf("\n");
                }
            }
            orre_wzx_free(w);
        }
        orre_model_free(&m);
    } else if (cmd == "emitterpath" && a + 4 < argc) {
        // emitterpath <move> <part> <entry> <frames>: one particle entry's emitter run on its own at the origin (its
        // mode word applied, the effect scale 0.75 on its node as a Pokémon of size class -1 gets it), two steps a
        // frame; per frame the live quads and where their centres and sizes are (for where a hit's burst sits)
        OrreWzx* w = orre_move_wzx(d, std::atoi(argv[a + 1]), 0xFFFFu, std::atoi(argv[a + 2]));
        const int k = std::atoi(argv[a + 3]);
        OrreWzxEntry e{};
        const OrreParticleBank* b = w && orre_wzx_entry(w, k, &e) && e.type == 3 ? orre_wzx_bank(w, k) : nullptr;
        if (!b) {
            std::printf("entry %d isn't a particle entry with a bank\n", k);
        } else {
            OrreParticleSystem* ps = orre_particles_create();
            orre_particles_reset(ps, 1);
            orre_particles_set_bank(ps, 1, b);
            const int h = orre_particles_start_link(ps, 1, e.emitter, (int)(e.mode & 1));
            orre_particles_node_flags(ps, h, e.mode);
            float pos0[3];
            orre_particles_node_position(ps, h, pos0);
            const float sc[3] = {0.75f, 0.75f, 0.75f};
            orre_particles_node_set(ps, h, 2, sc, (int)((e.mode >> 5) & 1), (int)((e.mode >> 9) & 1));
            std::printf("emitter %d mode 0x%X: the generator starts at %.3f %.3f %.3f\n", e.emitter, e.mode, pos0[0], pos0[1], pos0[2]);
            int first = 0;
            const int cnt = orre_particle_bank_count(b, &first);
            for (int i = 0; i < cnt; ++i) {
                OrreParticleEmitter em{};
                if (!orre_particle_emitter(b, i, &em) || !em.present || em.id != e.emitter) continue;
                std::printf("  type %d, kind 0x%08X, life %d (generator %d), gravity %g, friction %g, velocity %g %g %g, radius %g, "
                            "angle %g, random %g, size %g, params %g %g %g, %d code bytes\n",
                            em.type, em.kind, em.life, em.gen_life, em.gravity, em.friction, em.vel[0], em.vel[1], em.vel[2], em.radius,
                            em.angle, em.random, em.size, em.param[0], em.param[1], em.param[2], em.code_size);
            }
            const float view[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -100};
            const float proj[4] = {1, 0, 1, 0};
            for (int f = 0; f < std::atoi(argv[a + 4]); ++f) {
                orre_particles_reclaim(ps);
                orre_particles_step(ps);
                orre_particles_step(ps);
                const int n = orre_particles_quads(ps, view, proj, nullptr, 0);
                std::vector<OrreParticleQuad> q((size_t)std::max(0, n));
                orre_particles_quads(ps, view, proj, q.data(), n);
                float ylo = 1e30f, yhi = -1e30f, ysum = 0, big = 0;
                for (const auto& qq : q) {
                    const float cy = (qq.pos[0][1] + qq.pos[1][1] + qq.pos[2][1] + qq.pos[3][1]) * 0.25f;
                    ylo = std::min(ylo, cy); yhi = std::max(yhi, cy); ysum += cy;
                    big = std::max(big, std::fabs(qq.pos[0][1] - qq.pos[2][1]));
                }
                std::map<uint32_t, int> kinds;
                for (const auto& qq : q) ++kinds[qq.kind];
                // (with a sixth argument: every quad's centre and kind on the first frame)
                if (f == 0 && a + 5 < argc)
                    for (const auto& qq : q)
                        std::printf("    %08X centre %.2f %.2f %.2f, height %.2f\n", qq.kind, (qq.pos[0][0] + qq.pos[2][0]) * 0.5f,
                                    (qq.pos[0][1] + qq.pos[2][1]) * 0.5f, (qq.pos[0][2] + qq.pos[2][2]) * 0.5f, std::fabs(qq.pos[0][1] - qq.pos[2][1]));
                std::string ks;
                for (const auto& [kk, c] : kinds) { char t[32]; std::snprintf(t, sizeof t, " %08X x%d", kk, c); ks += t; }
                // the quads' sideways distance from the emitter (x, z) and size range, by kind: min / median / max (how
                // Charm's hearts were compared with the project's capture of the game's burst, 2026-11-21)
                for (const auto& [kk, c] : kinds) {
                    if (c < 3) continue;
                    std::vector<float> rd, sz;
                    for (const auto& qq : q) {
                        if (qq.kind != kk) continue;
                        const float cx = (qq.pos[0][0] + qq.pos[2][0]) * 0.5f, cz = (qq.pos[0][2] + qq.pos[2][2]) * 0.5f;
                        rd.push_back(std::sqrt(cx * cx + cz * cz));
                        // (the edge's length, not the corners' height difference: a turning quad's height changes)
                        const float ex = qq.pos[1][0] - qq.pos[0][0], ey = qq.pos[1][1] - qq.pos[0][1], ez = qq.pos[1][2] - qq.pos[0][2];
                        sz.push_back(std::sqrt(ex * ex + ey * ey + ez * ez));
                    }
                    std::sort(rd.begin(), rd.end());
                    std::sort(sz.begin(), sz.end());
                    std::printf("    kind %08X: sideways %.2f / %.2f / %.2f, size %.2f .. %.2f\n", kk, rd.front(), rd[rd.size() / 2],
                                rd.back(), sz.front(), sz.back());
                    // their mean prim and env colours (RGBA) (2026-11-21: Hidden Power's streaks are reddish in Studio
                    // and white in the game; the combining matches psSetupTev, so the colours themselves are compared)
                    double pr[4] = {}, en[4] = {};
                    int m = 0;
                    for (const auto& qq : q) {
                        if (qq.kind != kk) continue;
                        for (int c = 0; c < 4; ++c) { pr[c] += qq.prim[c]; en[c] += qq.env[c]; }
                        ++m;
                    }
                    std::printf("      prim %.0f %.0f %.0f %.0f, env %.0f %.0f %.0f %.0f\n", pr[0] / m, pr[1] / m, pr[2] / m, pr[3] / m,
                                en[0] / m, en[1] / m, en[2] / m, en[3] / m);
                    // their centres' mean / min / max height (per kind, to compare with the port's per-record
                    // particle log, ORRE_WAZA_SWEEP_PARTICLES; Grudge, 2026-11-21)
                    float hs = 0, hlo = 1e30f, hhi = -1e30f;
                    for (const auto& qq : q) {
                        if (qq.kind != kk) continue;
                        const float cy = (qq.pos[0][1] + qq.pos[1][1] + qq.pos[2][1] + qq.pos[3][1]) * 0.25f;
                        hs += cy; hlo = std::min(hlo, cy); hhi = std::max(hhi, cy);
                    }
                    std::printf("      height mean %.2f, min %.2f, max %.2f\n", hs / m, hlo, hhi);
                    // and the picture the first one draws: its group's GX format, size, and the picture's mean colour
                    // and alpha where it isn't see-through (how bright the texture term is in the TEV's mix)
                    for (const auto& qq : q) {
                        if (qq.kind != kk || !qq.textured) continue;
                        int fmt = -1, w = 0, hgt = 0, imgs = 0;
                        orre_particle_group(b, qq.group, &fmt, &w, &hgt, &imgs);
                        const OrreImage* im = orre_particle_group_image(b, qq.group, qq.image);
                        double rgb = 0, al = 0;
                        int lit = 0;
                        if (im && im->rgba)
                            for (int p = 0; p < im->width * im->height; ++p) {
                                const uint8_t* px = im->rgba + 4 * p;
                                al += px[3];
                                if (px[3] > 16) { rgb += (px[0] + px[1] + px[2]) / 3.0; ++lit; }
                            }
                        const int np = im ? im->width * im->height : 0;
                        std::printf("      picture: group %d image %d of %d, GX format %d, %dx%d; mean brightness %.0f where seen, "
                                    "mean alpha %.0f\n", qq.group, qq.image, imgs, fmt, w, hgt, lit ? rgb / lit : 0.0, np ? al / np : 0.0);
                        break;
                    }
                }
                if (n) std::printf("  frame %2d: %3d quads, centre y %.2f .. %.2f (mean %.2f), largest height %.2f; kinds%s\n", f, n, ylo, yhi,
                                   ysum / n, big, ks.c_str());
                else std::printf("  frame %2d: none\n", f);
            }
            orre_particles_free(ps);
        }
        orre_wzx_free(w);
    } else if (cmd == "meshes" && a + 1 < argc) {
        // meshes <species>: each mesh of the Pokémon's model with its drawing settings (render mode, PE block, alpha,
        // culling), for how it blends and writes depth
        OrreModel m{};
        if (orre_species_model(d, std::atoi(argv[a + 1]), 0, &m)) {
            for (int i = 0; i < m.mesh_count; ++i) {
                const OrreMesh& me = m.meshes[i];
                std::printf("mesh %2d: %4d vertices, render 0x%08X, alpha %.2f, cull %d, pe %d", i, me.vertex_count, me.render_mode, me.alpha,
                            me.cull, me.has_pe);
                if (me.has_pe)
                    for (int k = 0; k < 12; ++k) std::printf(" %02X", me.pe[k]);
                std::printf("%s%s\n", me.glow ? " glow" : "", me.hidden ? " hidden" : "");
            }
            orre_model_free(&m);
        }
    } else if (cmd == "wzxjoints" && a + 4 < argc) {
        // wzxjoints <move> <part> <entry> <frame>: a move model entry's joints at an animation frame: each joint's place
        // and its axes' lengths (its chain's scale), and which joint carries the owner (for checking carry entries)
        OrreWzx* w = orre_move_wzx(d, std::atoi(argv[a + 1]), 0xFFFFu, std::atoi(argv[a + 2]));
        const int k = std::atoi(argv[a + 3]);
        const float fr = (float)std::atof(argv[a + 4]);
        OrreWzxEntry e{};
        OrreModel m{};
        if (w && orre_wzx_entry(w, k, &e) && orre_wzx_model(w, k, &m)) {
            std::printf("entry %d: anim %d carry %d joint %d follow %d flags 0x%X, %d joints\n", k, e.model_anim, e.model_carry,
                        e.model_carry_joint, e.follow, e.model_flags, m.joint_count);
            std::vector<float> jw((size_t)m.joint_count * 12), js((size_t)m.joint_count * 3);
            if (orre_model_joint_world(&m, e.model_anim, fr, jw.data(), js.data()))
                for (int j = 0; j < m.joint_count; ++j) {
                    const float* J = &jw[(size_t)j * 12];
                    float len[3];
                    for (int c = 0; c < 3; ++c) len[c] = std::sqrt(J[c] * J[c] + J[4 + c] * J[4 + c] + J[8 + c] * J[8 + c]);
                    std::printf("  joint %d: at %.3f %.3f %.3f, scale %.3f %.3f %.3f%s\n", j, J[3], J[7], J[11], len[0], len[1],
                                len[2], j == e.model_carry_joint ? "  <- carries the owner" : "");
                }
            // each mesh's posed bounds at that frame (how Surf's shape-animated water was checked against the project's
            // capture: the waves' height range, 2026-11-21)
            std::vector<std::vector<OrreVertex>> pv((size_t)m.mesh_count);
            std::vector<OrreVertex*> pp((size_t)m.mesh_count);
            for (int i = 0; i < m.mesh_count; ++i) {
                pv[(size_t)i].resize((size_t)m.meshes[i].vertex_count);
                pp[(size_t)i] = pv[(size_t)i].data();
            }
            if (orre_model_pose_view(&m, e.model_anim, fr, nullptr, pp.data()))
                for (int i = 0; i < m.mesh_count; ++i) {
                    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
                    for (const OrreVertex& v : pv[(size_t)i])
                        for (int c = 0; c < 3; ++c) { lo[c] = std::min(lo[c], v.pos[c]); hi[c] = std::max(hi[c], v.pos[c]); }
                    std::printf("  mesh %d (%d vertices): x %.3f..%.3f  y %.3f..%.3f  z %.3f..%.3f\n", i, m.meshes[i].vertex_count, lo[0],
                                hi[0], lo[1], hi[1], lo[2], hi[2]);
                }
            orre_model_free(&m);
        }
        orre_wzx_free(w);
    } else if (cmd == "jointpos" && a + 4 < argc) {
        // jointpos <species> <animation> <frame> <joint>: a joint's place in the model's own space at that frame (for
        // checking against the game's battle-camera log, whose aim is the target's joint in the battle's space)
        OrreModel m{};
        if (orre_species_model(d, std::atoi(argv[a + 1]), 0, &m)) {
            std::vector<float> w((size_t)m.joint_count * 12), s((size_t)m.joint_count * 3);
            const int j = std::atoi(argv[a + 4]);
            if (j >= 0 && j < m.joint_count &&
                orre_model_joint_world(&m, std::atoi(argv[a + 2]), (float)std::atof(argv[a + 3]), w.data(), s.data()))
                std::printf("joint %d: %.4f %.4f %.4f (of %d joints)\n", j, w[(size_t)j * 12 + 3], w[(size_t)j * 12 + 7],
                            w[(size_t)j * 12 + 11], m.joint_count);
            orre_model_free(&m);
        }
    } else if (cmd == "wzxbolt" && a + 2 < argc) {
        // wzxbolt <move> <part>: a move file's bolts (screen effect kind 1): their settings, then the strips drawn on
        // a few frames (a camera at (0, 50, 150), the owner at the origin)
        OrreWzx* w = orre_move_wzx(d, std::atoi(argv[a + 1]), 0xFFFFu, std::atoi(argv[a + 2]));
        OrreWzxHeader h{};
        if (w && orre_wzx_header(w, &h))
            for (int k = 0; k < h.count; ++k) {
                OrreWzxBolt bp{};
                if (!orre_wzx_bolt(w, k, &bp)) continue;
                std::printf("entry %d: %d bolts over %d ticks, depth %d, height %g, width %g, branch %g, part %d, colour %02X%02X%02X%02X, "
                            "texture %dx%d\n  jitter %g %g %g, spread %g %g %g, step %g %g %g, scale %g %g %g\n",
                            k, bp.count, bp.frames, bp.depth, bp.height, bp.width, bp.branch, bp.part, bp.rgba[0], bp.rgba[1], bp.rgba[2],
                            bp.rgba[3], bp.tex_width, bp.tex_height, bp.jitter[0], bp.jitter[1], bp.jitter[2], bp.spread[0], bp.spread[1],
                            bp.spread[2], bp.step[0], bp.step[1], bp.step[2], bp.scale[0], bp.scale[1], bp.scale[2]);
                OrreBolt* bo = orre_bolt_create(&bp, 1);
                const float cam[3] = {0, 50, 150}, own[3] = {0, 0, 0};
                for (int f = 0; bo && orre_bolt_update(bo, 2); ++f)
                    if (f % 5 == 0) std::printf("  frame %d: %d strips\n", f, orre_bolt_quads(bo, cam, own, nullptr, nullptr, nullptr, 100000));
                orre_bolt_free(bo);
            }
        orre_wzx_free(w);
    } else if (cmd == "fsysmodel" && a + 2 < argc) {
        // fsysmodel <archive> <entry>: an HSD model read straight from an archive entry (a map: the arena), its size
        OrreModel m{};
        if (!orre_fsys_model(d, argv[a + 1], std::atoi(argv[a + 2]), &m)) {
            std::printf("not read\n");
        } else {
            long verts = 0;
            for (int i = 0; i < m.mesh_count; ++i) verts += m.meshes[i].vertex_count;
            std::printf("%d meshes, %ld vertices, %d textures, %d joints, %d animations; bounds %.1f %.1f %.1f .. %.1f %.1f %.1f\n",
                        m.mesh_count, verts, m.texture_count, m.joint_count, m.animation_count, m.bounds_min[0], m.bounds_min[1],
                        m.bounds_min[2], m.bounds_max[0], m.bounds_max[1], m.bounds_max[2]);
            // (with a third argument: each mesh's drawing settings and its height range, to find the floor and how
            // it writes depth)
            if (a + 3 < argc)
                for (int i = 0; i < m.mesh_count; ++i) {
                    const OrreMesh& me = m.meshes[i];
                    float lo = 1e30f, hi = -1e30f;
                    for (int v = 0; v < me.vertex_count; ++v) { lo = std::min(lo, me.vertices[v].pos[1]); hi = std::max(hi, me.vertices[v].pos[1]); }
                    std::printf("  mesh %d: %d vertices, y %.2f..%.2f, render 0x%X, pe %d, alpha %.2f%s%s\n", i, me.vertex_count, lo, hi,
                                me.render_mode, me.has_pe, me.alpha, me.glow ? " glow" : "", me.hidden ? " hidden" : "");
                }
            orre_model_free(&m);
        }
    } else if (cmd == "wzxlen" && a + 3 < argc) {
        // wzxlen <move> <species> <part>: how long the half's effects run (orre_wzx_run_ticks, 60 Hz ticks) and the
        // owner's animation for its slot, for checking against the move sweep's battle-camera logs (shot lengths)
        const int move = std::atoi(argv[a + 1]), sp = std::atoi(argv[a + 2]), part = std::atoi(argv[a + 3]);
        OrreSpecies s{};
        OrreModel m{};
        orre_species_get(d, sp, &s);
        orre_species_model(d, sp, 0, &m);
        OrreWzx* w = orre_move_wzx(d, move, s.model_id, part);
        OrreWzxHeader h{};
        if (w && orre_wzx_header(w, &h)) {
            int slot = h.slot;
            if (slot >= m.slot_count) slot = slot == 11 ? 0 : (slot >= 12 && slot <= 15 ? 1 : 0);
            slot = std::clamp(slot, 0, 16);
            const int an = orre_model_slot_animation(&m, slot);
            const float alen = an >= 0 ? orre_model_animation_length(&m, an) : 0.0f;
            std::printf("move %d part %d slot %d: effects %d ticks; animation %d frames (%d ticks); lead-in %d ticks\n", move,
                        part, slot, orre_wzx_run_ticks(w, m.slot_times[slot], 1200), (int)alen, 2 * (int)alen,
                        orre_wzx_lead_in(w, m.slot_times[slot]));
        } else {
            std::printf("no effect file for move %d part %d\n", move, part);
        }
        orre_wzx_free(w);
    } else if (cmd == "slots" && a + 1 < argc) {
        // slots <species|all>: each model's animation per slot (0-16: the model's own table), and for slots 8 and 9 (Hit,
        // Hit second motion) whether they're the same animation, or different animations with the same poses
        // (2026-11-22, snatcherLeo: "the Hit and then Hit, second motion, both are identical (it seems)")
        const bool all = std::strcmp(argv[a + 1], "all") == 0;
        const int n = orre_species_count(d);
        int same_index = 0, same_poses = 0, differ = 0, models = 0;
        for (int sp = all ? 0 : std::atoi(argv[a + 1]); sp < (all ? n : std::atoi(argv[a + 1]) + 1); ++sp) {
            OrreModel m{};
            if (!orre_species_model(d, sp, 0, &m)) continue;
            ++models;
            const int a8 = orre_model_slot_animation(&m, 8), a9 = orre_model_slot_animation(&m, 9);
            const char* verdict = "different";
            if (a8 >= 0 && a8 == a9) { verdict = "same animation"; ++same_index; }
            else if (a8 >= 0 && a9 >= 0) {
                // the same poses? every joint at every whole frame of the shorter one
                const float l8 = orre_model_animation_length(&m, a8), l9 = orre_model_animation_length(&m, a9);
                bool same = l8 == l9;
                std::vector<float> v8((size_t)m.joint_count * 9), v9(v8.size());
                for (int f = 0; same && f < (int)l8; ++f)
                    if (orre_model_joint_values(&m, a8, (float)f, v8.data()) && orre_model_joint_values(&m, a9, (float)f, v9.data()))
                        for (size_t k = 0; k < v8.size() && same; ++k) same = std::fabs(v8[k] - v9[k]) < 1e-4f;
                if (same) { verdict = "same poses"; ++same_poses; } else ++differ;
            } else {
                ++differ;
            }
            if (!all) {
                std::printf("species %d: %d animations\n", sp, m.animation_count);
                for (int s = 0; s <= 16; ++s) {
                    const int an = orre_model_slot_animation(&m, s);
                    std::printf("  slot %2d: animation %2d (%g frames)\n", s, an, an >= 0 ? orre_model_animation_length(&m, an) : 0.0f);
                }
                std::printf("slots 8 / 9: %s\n", verdict);
            }
            orre_model_free(&m);
        }
        if (all) std::printf("%d models: slots 8 and 9 the same animation %d, different animations with the same poses %d, different %d\n",
                             models, same_index, same_poses, differ);
    } else if (cmd == "leaves" && a + 4 < argc) {
        // leaves <move> <part> <entry> <effect scale>: a leaves effect run from the origin (Studio's own random
        // numbers, seed 1): every 4 battle frames, how many copies are alive, their mean and largest distance from the
        // origin and their mean height, and the first copy's place (2026-11-21: Feather Dance scatters wider in Studio
        // than the game's tight ball; this shows Studio's own numbers to compare with the game's code)
        OrreWzx* w = orre_move_wzx(d, std::atoi(argv[a + 1]), 0xFFFFu, std::atoi(argv[a + 2]));
        OrreWzxLeaves lp{};
        if (!w || !orre_wzx_leaves(w, std::atoi(argv[a + 3]), &lp)) {
            std::printf("no leaves effect there\n");
        } else {
            const float origin[3] = {0, 10, 0};
            OrreLeaves* lv = orre_leaves_create(&lp, origin, (float)std::atof(argv[a + 4]), 1);
            std::printf("count %d, frames %d, fade from %d, speed %.4f\n", lp.count, lp.frames, lp.fade_start, lp.speed);
            for (int f = 0; lv && f <= 75; ++f) {
                if (f % 4 == 0) {
                    float xf[32 * 12], alpha = 1;
                    const int n = orre_leaves_get(lv, xf, &alpha);
                    double sum = 0, mx = 0, hy = 0;
                    for (int k = 0; k < n; ++k) {
                        const float dx = xf[12 * k + 9] - origin[0], dy = xf[12 * k + 10] - origin[1], dz = xf[12 * k + 11] - origin[2];
                        const double r = std::sqrt(dx * dx + dy * dy + dz * dz);
                        sum += r;
                        mx = std::max(mx, r);
                        hy += xf[12 * k + 10];
                    }
                    std::printf("frame %2d: %2d alive, distance mean %.2f max %.2f, height mean %.2f, alpha %.2f; first at %.2f %.2f %.2f\n",
                                f, n, n ? sum / n : 0.0, mx, n ? hy / n : 0.0, alpha, n ? xf[9] : 0.0f, n ? xf[10] : 0.0f,
                                n ? xf[11] : 0.0f);
                }
                if (!orre_leaves_update(lv, 2)) break;
            }
            orre_leaves_free(lv);
        }
        orre_wzx_free(w);
    } else if (cmd == "wzxmodels" && a + 2 < argc) {
        // wzxmodels <move> <part>: a move file's model entries: each model's parts (vertices, texture stages, the
        // material's render mode and blend, glow / hidden), for looking into how one draws
        OrreWzx* w = orre_move_wzx(d, std::atoi(argv[a + 1]), 0xFFFFu, std::atoi(argv[a + 2]));
        OrreWzxHeader h{};
        if (w && orre_wzx_header(w, &h))
            for (int k = 0; k < h.count; ++k) {
                OrreWzxEntry e{};
                orre_wzx_entry(w, k, &e);
                OrreWzxBillboard bb{};
                const bool board = orre_wzx_billboard(w, k, &bb) != 0;
                OrreWzxLeaves lv{};
                if (e.type != 2 && !board && !orre_wzx_leaves(w, k, &lv)) continue;
                OrreModel m{};
                const int ok = orre_wzx_model(w, k, &m);
                std::printf("entry %d%s: archive %d bytes, anim %d tex %d carry %d joint %d flags 0x%X -> %s, %d meshes, %d textures, "
                            "%d animations, %d joints; bounds %.2f %.2f %.2f .. %.2f %.2f %.2f\n",
                            k, board ? " (billboard)" : "", e.model_size, board ? bb.anim : e.model_anim, e.model_tex_anim, e.model_carry,
                            e.model_carry_joint, e.model_flags,
                            ok ? "read" : "FAILED", m.mesh_count, m.texture_count, m.animation_count, m.joint_count, m.bounds_min[0],
                            m.bounds_min[1], m.bounds_min[2], m.bounds_max[0], m.bounds_max[1], m.bounds_max[2]);
                for (int i = 0; i < m.mesh_count; ++i) {
                    const OrreMesh& me = m.meshes[i];
                    std::printf("  mesh %d: %d vertices, render 0x%X, alpha %.2f, colours %d (first %.2f %.2f %.2f %.2f), stages %d", i,
                                me.vertex_count, me.render_mode, me.alpha, me.has_colours,
                                me.vertex_count ? me.vertices[0].clr[0] : 0.0f, me.vertex_count ? me.vertices[0].clr[1] : 0.0f,
                                me.vertex_count ? me.vertices[0].clr[2] : 0.0f, me.vertex_count ? me.vertices[0].clr[3] : 0.0f,
                                me.stage_count);
                    for (int s = 0; s < me.stage_count; ++s)
                        std::printf(" [tex %d flags 0x%X blend %.2f]", me.stages[s].texture, me.stages[s].flags, me.stages[s].blending);
                    std::printf("%s%s pe %d\n", me.glow ? " glow" : "", me.hidden ? " hidden" : "", me.has_pe);
                }
                // the material alpha its animation gives over time (every 5 frames), for checking fades in the model
                const int an = e.model_tex_anim >= 0 && e.model_tex_anim < m.animation_count ? e.model_tex_anim : 0;
                const float len = m.animation_count ? orre_model_animation_length(&m, an) : 0.0f;
                if (m.mesh_count && len > 0) {
                    std::vector<float> mat((size_t)m.mesh_count * ORRE_MATERIAL_FLOATS);
                    std::printf("  animation %d, %.0f frames; material alpha per mesh:", an, len);
                    for (float f = 0; f <= len + 0.01f; f += 5) {
                        orre_model_material_frame(&m, an, f, mat.data());
                        std::printf(" %.0f:", f);
                        for (int i = 0; i < m.mesh_count; ++i) std::printf("%s%.2f", i ? "/" : "", mat[(size_t)i * ORRE_MATERIAL_FLOATS + 3]);
                    }
                    std::printf("\n");
                }
                orre_model_free(&m);
            }
        orre_wzx_free(w);
    } else if (cmd == "wzxtex" && a + 3 < argc) {
        // wzxtex <move> <part> <out dir>: every particle bank's pictures in a move file, as PNGs
        // (<entry>_g<group>_<image>.png), with each emitter's kind bits listed (the mirror flags 0x20 / 0x40 among them)
        OrreWzx* w = orre_move_wzx(d, std::atoi(argv[a + 1]), 0xFFFFu, std::atoi(argv[a + 2]));
        OrreWzxHeader h{};
        if (w && orre_wzx_header(w, &h))
            for (int k = 0; k < h.count; ++k) {
                OrreWzxEntry e{};
                orre_wzx_entry(w, k, &e);
                if (e.type != 3 || !e.has_bank) continue;
                const OrreParticleBank* b = orre_wzx_bank(w, k);
                for (int g = 0; g < orre_particle_group_count(b); ++g) {
                    int fmt = 0, gw = 0, gh = 0, n = 0;
                    orre_particle_group(b, g, &fmt, &gw, &gh, &n);
                    for (int i = 0; i < n; ++i)
                        if (const OrreImage* im = orre_particle_group_image(b, g, i)) {
                            char path[512];
                            std::snprintf(path, sizeof path, "%s/%d_g%d_%d.png", argv[a + 3], k, g, i);
                            orre_png_write(path, im);
                        }
                    std::printf("entry %d group %d: format %d, %dx%d, %d images\n", k, g, fmt, gw, gh, n);
                }
                int first = 0;
                const int cnt = orre_particle_bank_count(b, &first);
                for (int i = 0; i < cnt; ++i) {
                    OrreParticleEmitter em{};
                    if (orre_particle_emitter(b, i, &em) && em.present)
                        std::printf("  emitter %d: kind 0x%08X group %d\n", em.id, em.kind, em.tex_group);
                }
            }
        orre_wzx_free(w);
    } else if (cmd == "wzxkinds" && a + 3 < argc) {
        // wzxkinds <move> <part> <steps>: every particle entry's emitter started at once on its own bank, run for
        // <steps> 60 Hz steps; then the live particles counted by (bank, picture group, kind & 0x60: the mirror flags)
        OrreWzx* w = orre_move_wzx(d, std::atoi(argv[a + 1]), 0xFFFFu, std::atoi(argv[a + 2]));
        OrreWzxHeader h{};
        OrreParticleSystem* ps = orre_particles_create();
        orre_particles_reset(ps, 1);
        std::map<const OrreParticleBank*, int> slots;
        if (w && orre_wzx_header(w, &h))
            for (int k = 0; k < h.count; ++k) {
                OrreWzxEntry e{};
                orre_wzx_entry(w, k, &e);
                const OrreParticleBank* b = e.type == 3 ? orre_wzx_bank(w, k) : nullptr;
                if (!b) continue;
                if (!slots.count(b)) {
                    const int s = (int)slots.size() + 1;
                    slots[b] = s;
                    orre_particles_set_bank(ps, s, b);
                }
                const int hnd = orre_particles_start_link(ps, slots[b], e.emitter, (int)(e.mode & 1));
                if (hnd) orre_particles_node_flags(ps, hnd, e.mode);
            }
        for (int t = 0; t < std::atoi(argv[a + 3]); ++t) {
            orre_particles_reclaim(ps);
            orre_particles_step(ps);
        }
        const float view[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -100};
        const float proj[4] = {1, 0, 1, 0};
        const int n = orre_particles_quads(ps, view, proj, nullptr, 0);
        std::vector<OrreParticleQuad> q((size_t)std::max(0, n));
        orre_particles_quads(ps, view, proj, q.data(), n);
        std::printf("%d live particles, %d drawn\n", orre_particles_count(ps), n);
        std::map<std::tuple<int, int, int>, int> hist;
        for (const auto& x : q) ++hist[{x.bank, x.group, (int)(x.kind & 0x60)}];
        for (const auto& [k, c] : hist)
            std::printf("bank %d group %d mirror 0x%02X: %d particles\n", std::get<0>(k), std::get<1>(k), std::get<2>(k), c);
        orre_particles_free(ps);
        orre_wzx_free(w);
    } else if (cmd == "bcamtest" && a + 1 < argc) {
        // bcamtest <log> [verbose]: Studio's battle camera (core battle_camera.cpp) against the game's own, shot by shot
        // and frame by frame, from OrrePort's ORRE_BCAM_LOG (docs/port/setup.md "The battle-camera log"): each computed
        // shot replayed from the logged inputs, seed and previous motion, then compared bit for bit (the log's "kh"
        // hex fields): the motion, the draws (the generator's state after), the 12 parameters, the zoom keys, and every
        // frame's eye, interest and zoom-key field of view. Authored shots (kind=anim) are skipped (not ported yet)
        auto kv = [](const std::string& line) {
            std::map<std::string, std::string> m;
            std::istringstream is(line);
            std::string tok;
            while (is >> tok) {
                const size_t eq = tok.find('=');
                if (eq != std::string::npos) m[tok.substr(0, eq)] = tok.substr(eq + 1);
            }
            return m;
        };
        auto hexes = [](const std::string& s) {
            std::vector<uint32_t> v;
            std::istringstream is(s);
            std::string t;
            while (std::getline(is, t, ',')) v.push_back((uint32_t)std::strtoul(t.c_str(), nullptr, 16));
            return v;
        };
        auto fl = [](uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; };
        auto bits = [](float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; };
        auto ints = [](const std::string& s) {
            std::vector<int> v;
            std::istringstream is(s);
            std::string t;
            while (std::getline(is, t, ',')) v.push_back(std::atoi(t.c_str()));
            return v;
        };
        const bool verbose = a + 2 < argc;
        std::ifstream in(argv[a + 1]);
        std::string line;
        OrreBattleCamera* cam = orre_bcam_create(1);
        OrreCamOwner own{};
        uint32_t flags = 0, seed = 0;
        int has_seq = 0, last = -1, shot = 0, live = 0, shots = 0, shots_ok = 0, frames = 0, frames_ok = 0, skipped = 0;
        bool shot_bad = false;
        std::string first_bad, prev_eye, prev_interest, prev_fov;
        auto note = [&](const std::string& what) {
            if (!shot_bad) first_bad = what;
            shot_bad = true;
        };
        auto check3 = [&](const char* name, const float* ours, const std::string& theirs, int n) {
            const auto v = hexes(theirs);
            for (int k = 0; k < n && k < (int)v.size(); ++k)
                if (bits(ours[k]) != v[k]) {
                    char b[256];
                    std::snprintf(b, sizeof b, "%s[%d] ours %.6f (%08x) game %.6f (%08x)", name, k, ours[k], bits(ours[k]), fl(v[k]), v[k]);
                    note(b);
                    return false;
                }
            return true;
        };
        auto finish = [&]() {
            if (!live) return;
            ++shots;
            if (!shot_bad) ++shots_ok;
            else std::printf("shot %d: %s\n", shot, first_bad.c_str());
            live = 0;
        };
        while (std::getline(in, line)) {
            const size_t at = line.find("[bcam] ");
            if (at == std::string::npos) continue;
            const std::string body = line.substr(at + 7);
            const std::string kind = body.substr(0, body.find(' '));
            auto m = kv(body);
            if (kind == "start") {
                finish();
                shot = std::atoi(m["shot"].c_str());
                own = OrreCamOwner{};
                const auto p = hexes(m["posh"]), r = hexes(m["roth"]), mn = hexes(m["bminh"]), mx = hexes(m["bmaxh"]);
                for (int k = 0; k < 3 && k < (int)p.size(); ++k) own.pos[k] = fl(p[k]);
                if (r.size() > 1) own.rot_y = fl(r[1]);
                for (int k = 0; k < 3 && k < (int)mn.size() && k < (int)mx.size(); ++k) { own.box_min[k] = fl(mn[k]); own.box_max[k] = fl(mx[k]); }
                own.size_class = std::atoi(m["size_class"].c_str());
                own.facing = std::atoi(m["facing"].c_str());
                own.is_pokemon = std::atoi(m["is_pokemon"].c_str());
                own.species = std::atoi(m["model_id"].c_str());
                own.slot = std::atoi(m["slot"].c_str());
                own.tp_count = std::atoi(m["tp_count"].c_str());
                const auto tp = ints(m["tp"]);
                for (int k = 0; k < 16 && k < (int)tp.size(); ++k) own.tp[k] = tp[k];
                has_seq = m["seq"] == "move" ? 1 : 0;
                flags = has_seq ? (uint32_t)std::strtoul(m["flags"].c_str(), nullptr, 16) : 0;
                last = std::atoi(m["last_motion"].c_str());
                seed = (uint32_t)std::strtoul(m["seed"].c_str(), nullptr, 16);
                // (the move viewer's room 999 runs the sequences before the camera; a battle the other way round)
                orre_bcam_set_order(cam, m["room"] == "999" ? 0 : 1);
            } else if (kind == "aim") {
                own.partners = m["partners"] == "-" ? 0 : std::atoi(m["partners"].c_str());
                const auto v = hexes(m["aimh"]);
                for (int k = 0; k < 3 && k < (int)v.size(); ++k) own.aim[k] = fl(v[k]);
            } else if (kind == "picked") {
                if (m["kind"] != "proc") { ++skipped; continue; }
                live = 1;
                shot_bad = false;
                orre_bcam_reset(cam, seed);
                orre_bcam_set_last_motion(cam, last);
                const int motion = orre_bcam_start(cam, &own, has_seq, flags);
                if (motion != std::atoi(m["motion"].c_str())) note("motion ours " + std::to_string(motion) + " game " + m["motion"]);
                float pr[12], ky[8], eye[3], itr[3], fov = 0;
                uint32_t se = 0;
                orre_bcam_debug(cam, pr, ky, &se);
                prev_eye = m["eyeh"];
                prev_interest = m["interesth"];
                prev_fov = m["fovh"];
                if (se !=(uint32_t)std::strtoul(m["seed_end"].c_str(), nullptr, 16)) {
                    char b[96];
                    std::snprintf(b, sizeof b, "draws: seed after ours %08x game %s", se, m["seed_end"].c_str());
                    note(b);
                }
                check3("params", pr, m["paramsh"], 12);
                const float k0[2] = {ky[0], ky[1]}, k1[2] = {ky[4], ky[5]};
                check3("fk0", k0, m["fk0h"], 2);
                check3("fk1", k1, m["fk1h"], 2);
                orre_bcam_get(cam, eye, itr, &fov);
                check3("start eye", eye, m["eyeh"], 3);
                check3("start interest", itr, m["interesth"], 3);
                if (verbose) std::printf("shot %d: motion %d\n", shot, motion);
            } else if (kind == "frame" && live) {
                const int step = std::atoi(m["step"].c_str());
                // (the log prints a line per game frame, but the camera only updates on some of them: lines where the
                // game's camera didn't change at all are frames without a camera update, so ours doesn't run either;
                // a "part=-" line that did change ran only the camera's own update)
                // (the FOV key is the sequence's half, so it can change on a frame whose shot didn't step: a shot's
                // first frame; "moved" is the shot itself: eye, interest, applied fov)
                const bool moved = m["eyeh"] != prev_eye || m["interesth"] != prev_interest || m["fovh"] != prev_fov;
                prev_eye = m["eyeh"];
                prev_interest = m["interesth"];
                prev_fov = m["fovh"];
                // (a frame that logs a target ran the sequence's aim even when nothing visible changed yet: the aim's
                // step lands on the next frame in a battle. A shot's first frame logs a target with the eye held: the
                // shot was started after that frame's camera step, so only the aim runs)
                const auto v = hexes(m["aimh"]);
                float aim[3] = {0, 0, 0};
                for (int k = 0; k < 3 && k < (int)v.size(); ++k) aim[k] = fl(v[k]);
                if (m["part"] == "-" && !moved) {
                } else if (m["part"] == "-") {
                    orre_bcam_update_camera_only(cam, step);
                } else if (!moved) {
                    orre_bcam_update_aim_only(cam, step, aim);
                } else {
                    orre_bcam_update(cam, step, aim);
                }
                float eye[3], itr[3], fov = 0;
                orre_bcam_get(cam, eye, itr, &fov);
                ++frames;
                const bool was = shot_bad;
                const std::string n = " (frame n=" + m["n"] + ")";
                bool ok = check3(("eye" + n).c_str(), eye, m["eyeh"], 3);
                ok = check3(("interest" + n).c_str(), itr, m["interesth"], 3) && ok;
                ok = check3(("fov" + n).c_str(), &fov, m["fov_keyh"], 1) && ok;
                if (ok) ++frames_ok;
                (void)was;
            } else if (kind == "end") {
                finish();
            }
        }
        finish();
        std::printf("computed shots: %d of %d exact; frames: %d of %d exact; authored shots skipped: %d\n", shots_ok, shots,
                    frames_ok, frames, skipped);
        orre_bcam_free(cam);
    } else if (cmd == "wzxscan") {
        // wzxscan: every move's three effect files read, and every particle entry's emitter run for 240 steps on its
        // own (the robustness check before the Moves preview plays them; release step 4)
        int files = 0, entries = 0, ptcl = 0, started = 0, kinds_app = 0, kinds_cam = 0, peak = 0, top_app = 0, top_plain = 0;
        int by_type[8] = {};
        for (int move = 1; move < 360; ++move)
            for (int part = 1; part <= 3; ++part) {
                OrreWzx* w = orre_move_wzx(d, move, 0, part);
                if (!w) continue;
                ++files;
                OrreWzxHeader h{};
                orre_wzx_header(w, &h);
                for (int k = 0; k < h.count; ++k) {
                    OrreWzxEntry e{};
                    orre_wzx_entry(w, k, &e);
                    ++entries;
                    ++by_type[e.type & 7];
                    if (e.type != 3) continue;
                    ++ptcl;
                    const OrreParticleBank* b = orre_wzx_bank(w, k);
                    if (!b) continue;
                    int first = 0;
                    const int cnt = orre_particle_bank_count(b, &first);
                    for (int i = 0; i < cnt; ++i) {
                        OrreParticleEmitter em{};
                        if (orre_particle_emitter(b, i, &em) && em.present) {
                            if (em.kind & 0x20000) ++kinds_app;
                            if (em.kind & 0x10000) ++kinds_cam;
                        }
                    }
                    {
                        // the started emitter itself (not its children): does it carry an application SRT?
                        OrreParticleEmitter em{};
                        if (e.emitter >= first && orre_particle_emitter(b, e.emitter - first, &em) && em.present) {
                            if (em.kind & 0x20000) ++top_app;
                            else ++top_plain;
                        }
                    }
                    OrreParticleSystem* ps = orre_particles_create();
                    orre_particles_reset(ps, 1);
                    orre_particles_set_bank(ps, 0, b);
                    const int hnd = orre_particles_start_link(ps, 0, e.emitter, (int)(e.mode & 1));
                    if (hnd) {
                        ++started;
                        orre_particles_node_flags(ps, hnd, e.mode);
                        for (int t = 0; t < 240; ++t) {
                            orre_particles_reclaim(ps);
                            orre_particles_step(ps);
                            peak = std::max(peak, orre_particles_count(ps));
                        }
                    } else {
                        std::printf("move %d part %d entry %d: emitter %d not started\n", move, part, k, e.emitter);
                    }
                    orre_particles_free(ps);
                }
                orre_wzx_free(w);
            }
        std::printf("%d files, %d entries (value %d, model %d, particles %d, effect %d, sound %d, control %d); %d particle "
                    "entries started; emitters with an application SRT %d, camera basis %d; peak live %d\n",
                    files, entries, by_type[1], by_type[2], by_type[3], by_type[4], by_type[5], by_type[6], started,
                    kinds_app, kinds_cam, peak);
        std::printf("started emitters with an application SRT %d, without %d\n", top_app, top_plain);
    } else if (cmd == "forms" && a + 1 < argc) {
        // forms <species>: each form's model archive
        const int sp = std::atoi(argv[a + 1]);
        for (int f = 0; f < orre_species_form_count(d, sp); ++f) {
            char name[64] = "";
            orre_species_form_archive(d, sp, f, name, sizeof name);
            std::printf("%d %s\n", f, name);
        }
    } else if (cmd == "dump" && a + 2 < argc) {
        const std::string what = argv[a + 1];
        const char* arg = argv[a + 2];
        if (what == "species") {
            if (std::strcmp(arg, "all") == 0)
                for (int i = 0; i < orre_species_count(d); ++i) dump_species(d, i);
            else
                dump_species(d, std::atoi(arg));
        } else if (what == "ability") {
            uint32_t nm = 0, doc = 0;
            if (orre_ability_messages(d, std::atoi(arg), &nm, &doc)) {
                json_str(orre_text(d, nm));
                std::printf(" ");
                json_str(orre_text(d, doc));
                std::putchar('\n');
            }
        } else if (what == "typechart") {
            // dump typechart all: 18 rows (attacker) x 18 columns (defender), in tenths
            for (int at = 0; at < 18; ++at) {
                for (int de = 0; de < 18; ++de) std::printf("%3d", orre_type_effect(d, at, de));
                std::putchar('\n');
            }
        } else if (what == "moves") {
            // dump moves all: one JSON line per move record, the fields the golden tests compare with the Python
            // extractor's decoder (tools/local/extract.py, moves_decoded.csv), plus each species' machine and egg lists
            for (int i = 0; i < orre_move_count(d); ++i) {
                OrreMove mv{};
                if (!orre_move_get(d, i, &mv)) break;
                std::printf("{\"index\": %d, \"name\": ", i);
                json_str(orre_text(d, mv.name_msg));
                std::printf(", \"type\": %d, \"power\": %d, \"accuracy\": %d, \"pp\": %d, \"priority\": %d, \"effect\": %d, "
                            "\"chance\": %d, \"target\": %d, \"contact\": %d, \"wzx\": %u, \"description\": ",
                            mv.type, mv.power, mv.accuracy, mv.pp, mv.priority, mv.effect, mv.effect_chance, mv.target,
                            mv.contact, mv.wzx_id);
                json_str(orre_text(d, mv.desc_msg));
                std::printf("}\n");
            }
        } else if (what == "effects") {
            // dump effects <model id|none>: for every move, the effect the given Pokémon model plays (its own version
            // first, then the general one): move, own (0/1), archive, entry id, the user's animation slot
            const uint32_t model = std::strcmp(arg, "none") == 0 ? 0xFFFFFFFFu : (uint32_t)std::strtoul(arg, nullptr, 0);
            for (int i = 1; i < orre_move_count(d); ++i) {
                OrreMoveEffect fx{};
                if (orre_move_effect(d, i, model, &fx))
                    std::printf("%d %d %s %u %d\n", i, fx.own_version, fx.archive, fx.entry_id, fx.slot);
                else
                    std::printf("%d - - - -\n", i);
            }
        } else if (what == "particles") {
            // dump particles <species> [out dir]: the PKX's own particle bank (2026-11-19): one line per emitter (index,
            // id, header fields, byte-code size), one per texture group (format, size, images, and each image's RGBA
            // checksum); with an out dir, every image as ptcl_<group>_<image>.png (the player's own data, written only
            // where asked)
            OrreModel m{};
            if (!orre_species_model(d, std::atoi(arg), 0, &m) || !m.particles) {
                std::printf("no particle bank\n");
            } else {
                int first = 0;
                const int n = orre_particle_bank_count(m.particles, &first);
                std::printf("bank first %d emitters %d groups %d\n", first, n, orre_particle_group_count(m.particles));
                for (int i = 0; i < n; ++i) {
                    OrreParticleEmitter e{};
                    orre_particle_emitter(m.particles, i, &e);
                    if (!e.present) { std::printf("emitter %d none\n", i); continue; }
                    std::printf("emitter %d id %d type %u tex %u genlife %u life %u kind %u f %g %g %g %g %g %g %g %g %g %g %g %g code %d ref %u\n",
                                i, e.id, e.type, e.tex_group, e.gen_life, e.life, e.kind, e.gravity, e.friction, e.vel[0], e.vel[1],
                                e.vel[2], e.radius, e.angle, e.random, e.size, e.param[0], e.param[1], e.param[2], e.code_size,
                                orre_particle_bank_ref(m.particles, i));
                }
                for (int g = 0; g < orre_particle_group_count(m.particles); ++g) {
                    int fmt = 0, w = 0, h = 0, k = 0;
                    orre_particle_group(m.particles, g, &fmt, &w, &h, &k);
                    std::printf("group %d fmt %d %dx%d images %d", g, fmt, w, h, k);
                    for (int i = 0; i < k; ++i) {
                        const OrreImage* im = orre_particle_group_image(m.particles, g, i);
                        uint32_t sum = 0;
                        if (im)
                            for (int p = 0; p < im->width * im->height * 4; ++p) sum = sum * 31 + im->rgba[p];
                        std::printf(" %08X", sum);
                        if (im && a + 3 < argc) {
                            char path[512];
                            std::snprintf(path, sizeof path, "%s/ptcl_%d_%d.png", argv[a + 3], g, i);
                            orre_png_write(path, im);
                        }
                    }
                    std::printf("\n");
                }
            }
            orre_model_free(&m);
        } else if (what == "machines") {
            // dump machines all: TM01..TM50, HM01..HM08 -> move id
            for (int k = 0; k < 58; ++k) std::printf("%d\n", orre_machine_move(d, k));
        } else if (what == "text") {
            json_str(orre_text(d, (uint32_t)std::strtoul(arg, nullptr, 0)));
            std::putchar('\n');
        } else if ((what == "mugshot" && a + 5 < argc) || (what == "pc" && a + 4 < argc)) {
            OrreImage img{};
            const int sp = std::atoi(arg);
            const bool shiny = shiny_arg(argv[a + 3]);
            const bool ok = what == "mugshot" ? orre_species_mug_shot(d, sp, shiny, std::atoi(argv[a + 4]), &img)
                                              : orre_species_pc_picture(d, sp, shiny, &img);
            const char* out = argv[what == "mugshot" ? a + 5 : a + 4];
            if (!ok || !orre_png_write(out, &img)) {
                std::fprintf(stderr, "orrestudio-cli: no such picture\n");
                rc = 1;
            }
            orre_image_free(&img);
        } else if (what == "cry" && (a + 3 < argc || (a + 2 < argc && std::strcmp(arg, "all") == 0))) {
            // dump cry <index> <out.wav>: the species' cry as the game plays it (cry.cpp), 16-bit mono WAV; with
            // "all" as the index, one line per species instead: index, sound id, rate, samples (no files)
            if (std::strcmp(arg, "all") == 0) {
                const int n = orre_species_count(d);
                int got = 0;
                for (int sp = 0; sp < n; ++sp) {
                    OrreSpecies s{};
                    OrreSound snd{};
                    if (!orre_species_get(d, sp, &s)) continue;
                    const bool ok = orre_species_cry(d, sp, &snd) != 0;
                    got += ok;
                    std::printf("%d %u %d %d%s\n", sp, (unsigned)s.cry, snd.rate, snd.count, ok ? "" : " (none)");
                    orre_sound_free(&snd);
                }
                std::fprintf(stderr, "%d cries read\n", got);
            } else {
                OrreSound snd{};
                if (!orre_species_cry(d, std::atoi(arg), &snd)) {
                    std::fprintf(stderr, "orrestudio-cli: no cry\n");
                    rc = 1;
                } else if (FILE* f = std::fopen(argv[a + 3], "wb")) {
                    // RIFF / WAVE, PCM 16-bit mono, little-endian (every platform writes it the same way here)
                    auto u32le = [&](uint32_t v) { for (int k = 0; k < 4; ++k) std::fputc((v >> (8 * k)) & 0xFF, f); };
                    auto u16le = [&](uint16_t v) { std::fputc(v & 0xFF, f); std::fputc(v >> 8, f); };
                    const uint32_t bytes = (uint32_t)snd.count * 2;
                    std::fwrite("RIFF", 1, 4, f); u32le(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
                    u32le(16); u16le(1); u16le(1); u32le((uint32_t)snd.rate); u32le((uint32_t)snd.rate * 2); u16le(2); u16le(16);
                    std::fwrite("data", 1, 4, f); u32le(bytes);
                    for (int k = 0; k < snd.count; ++k) u16le((uint16_t)snd.pcm[k]);
                    std::fclose(f);
                    std::printf("%d Hz, %d samples (%.2f s)\n", snd.rate, snd.count, snd.count / (double)snd.rate);
                } else {
                    rc = 1;
                }
                orre_sound_free(&snd);
            }
        } else if (what == "joints" && a + 4 < argc) {
            // dump joints <species> <animation|idle> <frame,frame,...>: each joint's 9 local values per frame
            OrreModel m{};
            if (!orre_species_model(d, std::atoi(arg), 0, &m)) {
                std::fprintf(stderr, "orrestudio-cli: no model\n");
                rc = 1;
            } else {
                const int anim = std::strcmp(argv[a + 3], "idle") == 0 ? m.idle_animation : std::atoi(argv[a + 3]);
                std::printf("animations %d idle %d animation %d length %g\n", m.animation_count, m.idle_animation, anim,
                            orre_model_animation_length(&m, anim));
                std::string frames = argv[a + 4];
                std::vector<float> vals((size_t)m.joint_count * 9);
                for (size_t p = 0; p <= frames.size();) {
                    const size_t q = std::min(frames.find(',', p), frames.size());
                    const float f = std::strtof(frames.substr(p, q - p).c_str(), nullptr);
                    p = q + 1;
                    orre_model_joint_values(&m, anim, f, vals.data());
                    std::printf("frame %g\n", f);
                    for (int j = 0; j < m.joint_count; ++j) {
                        std::printf("%d", j);
                        for (int k = 0; k < 9; ++k) std::printf(" %.6f", vals[(size_t)j * 9 + k]);
                        std::printf("\n");
                    }
                }
            }
            orre_model_free(&m);
        } else if (what == "textures" && a + 3 < argc) {
            // dump textures <species> <out dir>: every picture of the battle model as tex<N>.png (for looking into how
            // a model is drawn; the player's own data, written only where asked)
            OrreModel m{};
            if (!orre_species_model(d, std::atoi(arg), 0, &m)) {
                std::fprintf(stderr, "orrestudio-cli: no model\n");
                rc = 1;
            } else {
                for (int i = 0; i < m.texture_count; ++i) {
                    const std::string p = std::string(argv[a + 3]) + "/tex" + std::to_string(i) + ".png";
                    orre_png_write(p.c_str(), &m.textures[i]);
                    std::printf("tex%d %dx%d\n", i, m.textures[i].width, m.textures[i].height);
                }
            }
            orre_model_free(&m);
        } else if (what == "texanim" && a + 3 < argc) {
            // dump texanim <species> <frame,frame,...>: each textured stage's picture, blend factor and coordinate
            // transform during the idle animation (to see how eyes and mouths are chosen)
            OrreModel m{};
            if (!orre_species_model(d, std::atoi(arg), 0, &m)) {
                std::fprintf(stderr, "orrestudio-cli: no model\n");
                rc = 1;
            } else {
                const size_t n = (size_t)m.mesh_count * ORRE_MAX_STAGES;
                std::vector<int> tex(n);
                std::vector<float> blend(n), xf(n * 4);
                std::vector<uint8_t> shown((size_t)m.mesh_count);
                std::string frames = argv[a + 3];
                for (size_t p = 0; p <= frames.size();) {
                    const size_t q = std::min(frames.find(',', p), frames.size());
                    const float f = std::strtof(frames.substr(p, q - p).c_str(), nullptr);
                    p = q + 1;
                    orre_model_texture_frame2(&m, m.idle_animation, f, tex.data(), blend.data(), xf.data());
                    orre_model_visibility(&m, m.idle_animation, f, shown.data());
                    std::printf("frame %g\n", f);
                    for (int i = 0; i < m.mesh_count; ++i) {
                        // (untextured parts too, for their shown flag: Lunatone's mouth, 2026-11-19)
                        std::printf("  mesh %d shown %d hidden %d:", i, shown[(size_t)i], m.meshes[i].hidden);
                        for (int s = 0; s < m.meshes[i].stage_count; ++s) {
                            const size_t k = (size_t)i * ORRE_MAX_STAGES + s;
                            // (flags: the stage's TObjDesc +0x40 word, bits 4-8 the lighting channel, 16-19 the colour
                            // op; tev: a TEV override is present; to see why a layer is drawn or skipped)
                            std::printf(" [map %u tex %d blend %.2f flags 0x%X%s xf %.3f %.3f %.3f %.3f]", m.meshes[i].stages[s].map_id,
                                        tex[k], blend[k], m.meshes[i].stages[s].flags, m.meshes[i].stages[s].has_tev ? " tev" : "",
                                        xf[4 * k], xf[4 * k + 1], xf[4 * k + 2], xf[4 * k + 3]);
                        }
                        std::printf("\n");
                    }
                }
            }
            orre_model_free(&m);
        } else if (what == "model" && a + 4 < argc) {
            OrreModel m{};
            if (!orre_species_model(d, std::atoi(arg), shiny_arg(argv[a + 3]), &m)) {
                std::fprintf(stderr, "orrestudio-cli: no model\n");
                rc = 1;
            } else if (std::FILE* f = std::fopen(argv[a + 4], "w")) {
                std::fprintf(f, "joints %d meshes %d textures %d\n", m.joint_count, m.mesh_count, m.texture_count);
                for (int i = 0; i < m.mesh_count; ++i) {
                    const OrreMesh& me = m.meshes[i];
                    std::fprintf(f, "mesh %d tex %d hidden %d verts %d\n", i, me.texture >= 0 ? 1 : 0, me.hidden, me.vertex_count);
                    // the first stage's transform applied, as colo/hsdmodel.py's OBJ export does (the golden test)
                    const OrreTexStage* st = me.stage_count ? &me.stages[0] : nullptr;
                    for (int k = 0; k < me.vertex_count; ++k) {
                        const OrreVertex& v = me.vertices[k];
                        const float u = st ? (v.uv[0] + st->tu) * st->su : v.uv[0], w = st ? (v.uv[1] + st->tv) * st->sv : v.uv[1];
                        std::fprintf(f, "%.4f %.4f %.4f %.4f %.4f\n", v.pos[0], v.pos[1], v.pos[2], u, w);
                    }
                }
                std::fclose(f);
                // the materials, one line per mesh, for looking into how a model is drawn (stdout)
                for (int i = 0; i < m.mesh_count; ++i) {
                    const OrreMesh& me = m.meshes[i];
                    std::printf("mesh %2d: render %08X pe %d", i, me.render_mode, me.has_pe);
                    if (me.has_pe)
                        std::printf(" [blend type %u src %u dst %u, alpha test %u/%u op %u %u/%u, z %u flags %02X]", me.pe[4],
                                    me.pe[5], me.pe[6], me.pe[9], me.pe[1], me.pe[10], me.pe[11], me.pe[2], me.pe[8], me.pe[0]);
                    std::printf(" cull %d colours %d hidden %d", me.cull, me.has_colours, me.hidden);
                    for (int s = 0; s < me.stage_count; ++s) {
                        const OrreTexStage& t = me.stages[s];
                        std::printf(" | stage %d tex %d set %d coord %u light %02X colour %u alpha %u blend %.2f wrap %u/%u%s%s", s,
                                    t.texture, t.uv_set,
                                    t.flags & 15, t.flags >> 4 & 0x1F, t.flags >> 16 & 15, t.flags >> 20 & 15, t.blending, t.wrap_s,
                                    t.wrap_t, t.rotated ? " rotated" : "", t.has_tev ? " TEV" : "");
                    }
                    std::printf("\n");
                }
                std::printf("joints %d, meshes %d, textures %d\n", m.joint_count, m.mesh_count, m.texture_count);
            }
            orre_model_free(&m);
        } else {
            rc = usage();
        }
    } else {
        rc = usage();
    }
    orre_disc_close(d);
    return rc;
}
