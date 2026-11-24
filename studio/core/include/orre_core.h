/*
 * orre_core.h: the shared reader core's C API (OrrePort Studio; later also the port and XD's tools).
 *
 * Why a C API over C++ inside: the plan's decision 2 (Studio review, 2026-11-17): Studio's shells (Win32 now, Qt and
 * AppKit later) and the port all call one reader per disc format, so the boundary is plain C: opaque handles,
 * plain structs, UTF-8 strings, no exceptions or STL types crossing it.
 *
 * What it reads (first stretch): the disc image through nod (any format nod opens: ISO, GCM, RVZ, WIA, CISO...), the
 * fsys archives and their LZSS packing, the species table in common_rel, the message banks (names), the GS texture
 * files (mug shots, PC pictures) and the HSD battle models (bind pose). Everything is read from the player's own disc
 * at run time; nothing from the disc is ever written into the program or the repo.
 *
 * Platforms: plain C / C++ over nod; the same on Windows, Linux and macOS (likely: only Windows is built so far; no
 * OS calls are made here). Thread safety: one OrreDisc may be used from several threads; calls are serialised inside.
 */
#ifndef ORRE_CORE_H
#define ORRE_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OrreDisc OrreDisc;

/* Opens a disc image read-only (other programs, OrrePort included, may have it open at the same time). On failure
 * returns NULL and writes a plain-language reason into err (UTF-8). path is UTF-8. */
OrreDisc* orre_disc_open(const char* path, char* err, size_t err_len);
void orre_disc_close(OrreDisc* disc);
/* the disc's 6-letter game id ("GC6E01"), or "" */
const char* orre_disc_game_id(OrreDisc* disc);
/* main.dol's SHA-1 as 40 lowercase hex digits (the fingerprint OrrePort's disc check compares; the US release's is
 * config/GC6E01/build.sha1's main.dol line); 0 and "" when the disc has no main.dol */
int orre_disc_main_dol_sha1(OrreDisc* disc, char out[41]);
/* Whether a Colosseum disc's program file is the original release's (snatcherLeo, 2026-11-23: refuse a US, European or
 * Japanese disc whose fingerprint isn't the original's, in both apps): 1 it is, 0 it differs (another revision, or a
 * modified disc), -1 the game id has no known fingerprint. The table (disc.cpp) holds one main.dol SHA-1 per game id. */
int orre_disc_is_original(OrreDisc* disc);
/* The "which disc" rule both apps share (4.4, decided 2026-11-22: ROM\ is the one place for both apps' discs, and one
 * rule picks a disc there; the project's docs/port/disc-sync-review.md): the disc image of game id `game_id` ("GC6E01" in
 * ROM\, "GC6P01" or "GC6J01" in ROM Other Regions\) in `folder` (UTF-8). Files ending ".part" (a copy in progress) are
 * skipped; every other file is opened to read its game id. With two or more matches, the newest (last change) wins, then
 * the first by name. Writes the chosen file's full path (UTF-8) into out; returns how many matched (0: none, out ""). */
int orre_disc_find(const char* folder, const char* game_id, char* out, size_t out_len);
/* The same rule, refusing discs whose program file isn't the original release's (orre_disc_is_original == 0; snatcherLeo,
 * 2026-11-23: refuse a modified US, European or Japanese disc in both apps): the newest original wins, and the newest
 * refused one (if any) is written into `refused` so the app can say why it isn't used. Returns how many originals
 * matched. orre_disc_find is this with no refused list, and also refuses them (updated 2026-11-23). */
int orre_disc_find_checked(const char* folder, const char* game_id, char* out, size_t out_len, char* refused, size_t refused_len);

/* ---- species ---- */

/* One species record as the game stores it (common_rel's species table, 0x11C bytes per entry; layout: PokemonData in
 * include/game/pokemon_data.h, every offset checked at compile time in layout_check.c). Field names follow the
 * decomp's, in English. Message ids are resolved with orre_text. */
typedef struct OrreSpecies {
    int index;               /* internal species number (the table position; not the National Dex) */
    uint16_t national_dex;   /* +0x12 numPokemon */
    uint16_t hoenn_first;    /* +0x14 numZukan: Hoenn's order first, then the rest */
    uint8_t growth_rate;     /* +0x00 growDataId (EXP group) */
    uint8_t catch_rate;      /* +0x01 get */
    uint8_t gender_ratio;    /* +0x02 sexRatio */
    uint16_t exp_yield;      /* +0x06 giveExp */
    uint16_t base_friendship;/* +0x08 initFriend */
    uint16_t height;         /* +0x0A, tenths of a metre (likely) */
    uint16_t weight;         /* +0x0C, tenths of a kilogram (likely) */
    uint16_t cry;            /* +0x0E voice: sound id */
    uint32_t name_msg;       /* +0x18 */
    uint32_t category_msg;   /* +0x1C typeName ("Seed" for "Seed Pokémon") */
    uint32_t dex_text_msg;   /* +0x20 doc */
    uint32_t model_id;       /* +0x2C pkxDataId */
    uint8_t types[2];        /* +0x30 zokuseiDataId */
    uint8_t abilities[2];    /* +0x32 tokuseiDataId */
    uint16_t held_items[2];  /* +0x70 itemDataId */
    uint16_t base_stats[6];  /* +0x84 basis: HP, Attack, Defense, Sp. Atk, Sp. Def, Speed */
    uint16_t effort[6];      /* +0x90 giveEffort, same order */
    struct { uint8_t kind; uint16_t param; uint16_t target; } evolutions[5]; /* +0x9C sinka */
    struct { uint8_t level; uint16_t move; } level_moves[20];                /* +0xBA getWaza */
    struct { uint8_t colour; uint16_t mug_shot_id; uint32_t pc_picture_id; } faces[2]; /* +0x10C [0] normal, [1] shiny */
    /* the Moves tab's learnset (2026-11-19; include/game/pokemon_data.h, verified): */
    uint8_t machines[58];    /* +0x34 wazaMcn: TM01-50 then HM01-08, non-zero = can learn (orre_machine_move: which move) */
    uint16_t egg_moves[8];   /* +0x74 kowaza: egg move ids, 0 = empty */
} OrreSpecies;

int orre_species_count(OrreDisc* disc);
/* 1 on success */
int orre_species_get(OrreDisc* disc, int index, OrreSpecies* out);
/* the raw 0x11C-byte record (for "Advanced" and the golden tests); 1 on success */
int orre_species_raw(OrreDisc* disc, int index, uint8_t out[0x11C]);

/* ---- evolution methods, grouped (snatcherLeo, 2026-11-18: "I do want to keep track of this, and/or categorize it
 * internally, for later manipulation purposes") ----
 * An evolution slot's kind is Gen 3's evolution id; what each kind checks is verified from the game's own code
 * (pokemonEvolutionCheck, src/game/pokemon_range_80128524.c; docs/engine/evolution.md). The groups are Studio's own
 * names for editing; the kind stays the truth. */
typedef enum OrreEvoCategory {
    ORRE_EVO_NONE = 0,       /* kind 0: an empty slot */
    ORRE_EVO_LEVEL,          /* 4 (and 13, Gen 3's Ninjask kind, which Colosseum never checks): reach a level */
    ORRE_EVO_FRIENDSHIP,     /* 1, 2 (day), 3 (night): friendship 220+ on a level-up; 2 and 3 never happen here */
    ORRE_EVO_TRADE,          /* 5 trade, 6 trade holding an item */
    ORRE_EVO_ITEM,           /* 7: an item used on it (on this disc, evolution stones only: orrestudio-cli evolutions) */
    ORRE_EVO_STATS,          /* 8 / 9 / 10: a level, and Attack > / = / < Defense (Tyrogue) */
    ORRE_EVO_PERSONALITY,    /* 11 / 12: a level, split by the personality value (Wurmple) */
    ORRE_EVO_EXTRA,          /* 14: an extra Pokémon next to the main evolution, given a free party slot (Shedinja) */
    ORRE_EVO_BEAUTY          /* 15: the Beauty condition (Feebas) */
} OrreEvoCategory;
typedef enum OrreEvoTrigger { ORRE_EVO_ON_NONE = 0, ORRE_EVO_ON_LEVEL_UP, ORRE_EVO_ON_TRADE, ORRE_EVO_ON_ITEM } OrreEvoTrigger;
typedef enum OrreEvoParam { ORRE_EVO_PARAM_NONE = 0, ORRE_EVO_PARAM_LEVEL, ORRE_EVO_PARAM_ITEM, ORRE_EVO_PARAM_VALUE } OrreEvoParam;
OrreEvoCategory orre_evo_category(int kind);
/* when the game checks it: on a level-up, on a trade, or when an item is used (pokemonEvolutionCheck's modes 0, 2, 1) */
OrreEvoTrigger orre_evo_trigger(int kind);
/* what the slot's parameter means: a level, an item id, a value (Beauty), or nothing */
OrreEvoParam orre_evo_param(int kind);
/* 1 if Colosseum's code can ever fire it (0 for 2 and 3: no clock; 13: not handled; 0 for an empty slot) */
int orre_evo_works_in_colosseum(int kind);

/* ---- abilities and types (tables in main.dol) ---- */

/* An ability's name and description message ids (pokemon_tokusei_data, 0x8035F5E0: 12-byte records {pad, u32 name,
 * u32 description}; include/game/pokemon_data.h PokemonTokuseiData). 1 on success. */
int orre_ability_messages(OrreDisc* disc, int ability, uint32_t* name_msg, uint32_t* doc_msg);
int orre_ability_count(OrreDisc* disc);
/* How hard a move of type `attacker` hits a Pokémon of type `defender`, in tenths: 20 (x2), 10, 5 (x1/2) or 0, from
 * the type table's chart row (lbl_8035B500, 18 records of 0x2C bytes, u16[18] at +0x08: 0x41 super effective, 0x42 not
 * very effective, 0x43 no effect, 0x3F neutral; docs/engine/battle-types.md). -1 if out of range. */
int orre_type_effect(OrreDisc* disc, int attacker, int defender);
/* An item's name message id: item id -> slot through the lookup lbl_803681E8 (count lbl_80478BD8), then the 0x28-byte
 * record at lbl_80363CE8 (count lbl_80478BB0), name at +0x10 (docs/engine/pokemon-data.md "Item entry"; the
 * extractor's reader). 0 if unknown. */
uint32_t orre_item_name_msg(OrreDisc* disc, int item);
/* A type's move category from the type table (+0x00): 1 physical, 2 special, 0 neither ("???"); -1 if out of range */
int orre_type_category(OrreDisc* disc, int type);

/* ---- moves (the Moves tab, 2026-11-19) ----
 * The move table: common_rel section 4 +0x11C7C4, 358 records of 0x38 bytes (ids 0 empty, 1-354 Gen 3's moves, 355
 * empty, 356 Shadow Rush, 357 a placeholder; docs/engine/pokemon-data.md "Move entry", game-data-tables.md, verified).
 * Its kind (physical / special) is its type's (orre_type_category); a move with power 0 is a status move. */
typedef struct OrreMove {
    int index;
    int8_t priority;        /* +0x00 */
    uint8_t pp;             /* +0x01 */
    uint8_t type;           /* +0x02 */
    uint8_t target;         /* +0x03 target range */
    uint8_t accuracy;       /* +0x04 (moves that never miss store 100, as Gen 3 does) */
    uint8_t effect_chance;  /* +0x05 a secondary effect's chance */
    uint8_t contact;        /* +0x06 makes contact */
    int16_t power;          /* +0x16 (1 for moves whose damage is worked out otherwise) */
    uint16_t effect;        /* +0x1A the effect id: which battle script runs (Gen 3's 0-213) */
    uint32_t name_msg;      /* +0x20 */
    uint32_t desc_msg;      /* +0x2C (its bank is main.dol's) */
    uint32_t wzx_id;        /* +0x30 */
} OrreMove;
int orre_move_count(OrreDisc* disc);
/* 1 on success */
int orre_move_get(OrreDisc* disc, int move, OrreMove* out);
/* The move a TM or HM teaches: machine 0-49 = TM01-50, 50-57 = HM01-08 (main.dol item_waza_machine_no, 0x80368018:
 * 58 records of 8 bytes {u32, u32 move}; docs/engine/address-map.md). 0 if out of range. */
int orre_machine_move(OrreDisc* disc, int machine);

/* What a move shows on a Pokémon (docs/engine/models-animation.md "Move effects", wzx.md; verified from the code):
 * the game first looks for the Pokémon model's own version (main.dol lbl_80373750: 0x10-byte records {u8 variant,
 * u16 model id (pkxDataId), u16 move, u32 archive id, u32 entry id}, 367 of them), then the move's general one
 * (lbl_8036E150: one 0x1C record per move {u32 2000 + move, 3 x {u32 archive id, u32 entry id}}, part 1 = the user's
 * half). The WZX sequence header (after its 0x70-byte node header) starts with the animation slot the user plays. */
typedef struct OrreMoveEffect {
    int own_version;        /* 1: this Pokémon model has its own version of the move */
    char archive[48];       /* the WZX archive's name (wzx_<move>_...), "" when the move has none */
    uint32_t entry_id;
    int slot;               /* the user's animation slot (0-16; orre_model's slot_animation), -1 if none */
} OrreMoveEffect;
/* model_id: the species' model_id (pkxDataId). 1 when the move has an effect on the disc. */
int orre_move_effect(OrreDisc* disc, int move, uint32_t model_id, OrreMoveEffect* out);

/* ---- Move-effect files (WZX; wzx.cpp, 2026-11-19, release step 4) ----
 * A move's effect is a timeline of entries (docs/engine/wzx.md; read as wazaSequenceLoadData and its helpers do,
 * src/game/wazaSequence.c, verified from the code; the same walk as tools/local/colo/wzx.py, which reads all 1,098
 * files to their end). part: 1 = the user's half (_attack), 2 = the target's half (_damage), 3 = the field / self
 * half. orre_move_wzx looks for the Pokémon model's own version first, as orre_move_effect does. */
typedef struct OrreWzx OrreWzx;
OrreWzx* orre_move_wzx(OrreDisc* disc, int move, uint32_t model_id, int part);
/* a sequence file by its archive's name ("wzx_monsterball_downin": a ball's drop-in, the faint), NULL if none */
OrreWzx* orre_wzx_named(OrreDisc* disc, const char* name);
OrreWzx* orre_wzx_read(const uint8_t* data, size_t size); /* NULL when it doesn't read to its end */
void orre_wzx_free(OrreWzx* wzx);
typedef struct OrreWzxHeader {
    int slot;            /* the owner's animation slot (+0x00; 0x0B-0x0F fall back on short slot tables) */
    uint32_t flags;      /* +0x08, with the loader's fixes for older layouts (mode <= 3: | 0x78; mode <= 5: bits 15-20 cleared) */
    int value, mode;     /* +0x0C, +0x10 */
    int camera_size;     /* bytes of its own resource archive (the authored camera), 0 when none */
    int count;
} OrreWzxHeader;
int orre_wzx_header(const OrreWzx* wzx, OrreWzxHeader* out);
/* one entry: the node header (file +0x00..+0x6C) and its payload */
typedef struct OrreWzxEntry {
    int key, type;         /* type: 1 value, 2 model, 3 particles, 4 screen effect, 5 sound, 6 control */
    int attach;            /* +0x08: which attach point (0-15 the slot's, 16 the bounding box's centre) */
    int follow;            /* +0x0C: how a particle follows its joint (1 position, 2 rotation, 3 scale, 4 pos+rot,
                            * 5 rot+scale, 6 pos+scale, 7 all; 0 placed once at the owner) */
    int link_key, own_index, base_index, length_index; /* the start rule (+0x10..+0x1C) */
    int timing[16];        /* +0x20, 60 Hz frames as stored */
    uint32_t flags;        /* +0x60: 1 = attach to the linked model entry, 2 = place once, 4 = no attach (world), 8 = battle scale */
    int part;              /* +0x64: the linked model's part */
    int bank_link;         /* +0x6C: reuse that entry's particle bank (0 = its own) */
    /* payload */
    int value_kind, value; float value_f;          /* type 1 */
    int emitter;           /* type 3: the emitter id started (+0) */
    uint32_t mode;         /* type 3: the mode word (+4): bit 0 link 1, 1 children follow the joint, 2 / 4 offset
                            * rotation in local, 3 velocity rotation in local, 5 scale the generator, 7 texture
                            * scaling, 8 / 10 tornado scaling, 9 motion (gravity, friction) scaled, 11 node scaling,
                            * 12 random-velocity scaling (wazaSequenceEntry.c _wazaSequenceParticleEntryStart) */
    int has_bank;          /* type 3: carries its own GPT1 bank */
    int effect_kind, effect_frames;                 /* type 4 */
    int sound, sound_kind;                          /* type 5 */
    int control;                                    /* type 6 */
    int model_size;                                 /* type 2: the embedded model archive's size */
    /* type 2 (_wazaSequenceModelEntryLoad, verified): its joint and texture animations (-1 none; played once at
     * half rate: one frame per 30 fps battle frame), carry (the owner rides the effect model's joint
     * model_carry_joint: Fly, Dig, Bounce...), and flags (1 drawn after the owner's group, 2 hold at the end, 4 60 fps
     * animation, 8 carry: the owner keeps its displacement, 0x10 carry: pivot on the owner's centre, 0x20 shadows) */
    int model_anim, model_tex_anim, model_carry, model_carry_joint;
    uint32_t model_flags;
} OrreWzxEntry;
/* a model entry's model (the embedded HSD archive's scene_data), 1 on success; free with orre_model_free */
int orre_wzx_model(const OrreWzx* wzx, int index, struct OrreModel* out);
int orre_wzx_entry(const OrreWzx* wzx, int index, OrreWzxEntry* out);
/* A colour fade (screen effect kind 0; 955 on the disc): what it tints (0 a full-screen colour over the scene,
 * 1 the stage's models, 2 the owner Pokémon), whether the keys' alpha / RGB count, and its keys (colours as R, G, B, A;
 * len in 60 Hz ticks, -1 = hold for good). The tint multiplies a model's final colour by 2 x c / 255 (0x80 = none);
 * the full-screen colour blends over the scene by its alpha. */
typedef struct OrreWzxFade {
    int target, use_alpha, ignore_rgb, count;
    struct { uint8_t from[4], to[4]; int len; } key[16];
} OrreWzxFade;
int orre_wzx_fade(const OrreWzx* wzx, int index, OrreWzxFade* out); /* 1 when entry `index` is a colour fade */
/* its colour `ticks` after it started (60 Hz); 0 once it has run out (no tint any more) */
int orre_wzx_fade_colour(const OrreWzxFade* fade, int ticks, uint8_t rgba[4]);
/* A bolt (screen effect kind 1, boltEffectCreate; Thunder's lightning, its only user): forked strips drawn from a
 * height down onto the owner, revealed top-down and faded out, several one after another (count) over its frames.
 * Read from the descriptor as fn_801364A8 case 1 stores it (verified from the code); texture: its GStexture, decoded */
typedef struct OrreWzxBolt {
    float jitter[3];   /* each point's random wander per update (x randpm, cumulative) */
    float spread[3];   /* each new point's random step (x randpm) ... */
    float step[3];     /* ... plus this step (downwards: the bolt grows from (0, height, 0) until below 0) */
    float scale[3];    /* the local bolt's scale into the world */
    float height;      /* the start's height (local) and the reveal's range */
    float width;       /* the trunk's width (branches start at half their parent's, tapering) */
    float branch;      /* the chance a point forks, at depth 0 (falling with depth) */
    int count;         /* bolts, one after another */
    int depth;         /* the deepest fork */
    int part;          /* the owner's joint the trunk's tip lands on (> 0), else the owner's place */
    int frames;        /* the whole effect, in 60 Hz ticks */
    uint8_t rgba[4];   /* the strips' colour (texture x colour, added to the picture) */
    int tex_width, tex_height;
    const uint8_t* tex_rgba; /* owned by the WZX; NULL when the picture didn't decode */
} OrreWzxBolt;
int orre_wzx_bolt(const OrreWzx* wzx, int index, OrreWzxBolt* out); /* 1 when entry `index` is a bolt */
/* The bolts of one effect, run as boltStartEffect / _boltUpdate / _boltDrawStrip do (src/game/effect/effect_visual.c,
 * verified from the code), with Studio's own random numbers (the game draws from its shared sequence). update: one
 * battle frame (the game's frame step, 2 ticks), 0 once the effect has ended. quads: the visible strips as quads in
 * the world: 4 corners each (xyz), their texture coordinates and colour; camera: the eye in the world, owner: where the
 * trunk's tip lands (the owner's joint `part`, or its place). Returns the quads written (at most max) */
typedef struct OrreBolt OrreBolt;
OrreBolt* orre_bolt_create(const OrreWzxBolt* params, uint32_t seed);
void orre_bolt_free(OrreBolt* bolt);
int orre_bolt_update(OrreBolt* bolt, int ticks);
int orre_bolt_quads(const OrreBolt* bolt, const float camera[3], const float owner[3], float* xyz, float* uv, uint8_t* rgba, int max);
/* A blur (screen effect kind 9, blurEffectCreate; fn_80137114 reads it, verified from the code): screen = 1 lays the
 * last frame's picture over the new one (GSgfxBeginBackFBCapture / _blurDrawCapture), 0 draws the owner again at
 * earlier moments of its animation (after-images, _blurRender). Its keys run one after another: each goes from `from`
 * to `to` over `ticks` (60 Hz); the value is the after-images' amount (copies = 30 x value at the battle's rate) or
 * the picture's strength (alpha = 255 x value) */
typedef struct OrreWzxBlur {
    int screen, count;
    struct { float from, to; int ticks; } key[16];
} OrreWzxBlur;
int orre_wzx_blur(const OrreWzx* wzx, int index, OrreWzxBlur* out); /* 1 when entry `index` is a blur */
/* A billboard (screen effect kind 11, billboardEffectCreate; fn_801364A8 case 11 and _billboardUpdate, verified from
 * the code): a model held just in front of the camera and scaled to fill the view (the sandstorm). Its animation
 * (`anim`, played from frame 0 at rate 0.5: one frame a battle frame; `loop` 1 repeats it) runs until it ends; the
 * model is read with orre_wzx_model on the same entry. 1 when entry `index` is a billboard with a model */
typedef struct OrreWzxBillboard { int anim, loop, subid; } OrreWzxBillboard;
/* Leaves (screen effect kind 5, leaffxEffectCreate; fn_801364A8 case 5, leaffxStartEffect, _leaffxUpdate,
 * _leaffxMoveLeaf, _leaffxGenerateLeafData, all 100% matched): up to 32 copies of the entry's model (read with
 * orre_wzx_model on the same entry), each flying cubic Bezier curves from the owner's attach point, turning between
 * random orientations, sized scale (base + range x randpm) x the effect scale; slowing from twice the speed until
 * fade_start, then fading out until the effect's frames end; a leaf below the ground is dropped */
typedef struct OrreWzxLeaves {
    float scale_base, scale_range;   /* +0x00 / +0x04: each leaf's size */
    float near_base, near_range;     /* +0x08 / +0x0C: how far a curve's end point lies */
    float far_base, far_range;       /* +0x10 / +0x14: how far its control points lie */
    float lift;                      /* +0x18: each new end point raised by this */
    float speed;                     /* +0x1C: curve progress per tick */
    float first_base, first_range;   /* +0x20 / +0x24: the first end point's distance factor */
    int count, fade_start, frames, attach;  /* +0x28, +0x2C (ticks), the entry's frames (ticks), +0x30 */
} OrreWzxLeaves;
int orre_wzx_leaves(const OrreWzx* wzx, int index, OrreWzxLeaves* out); /* 1 when entry `index` is a leaves effect */
/* The env-map shine (screen effect kind 8, envMapEffectCreate; tracefx.c case 8, _envMapUpdate, GSmodelEnableEnvMap,
 * _matGSmatEnableEnvMapExt; verified from the code): every update the other visible models are drawn from the battle
 * camera into a screen-sized texture, which every material of the owner gets as one extra stage on the light map's
 * ext channel (flags 0x80): coordinates by mode (0 reflection = 1, 1 gradation = 5, 2 backlight = 6), its colour by
 * type (0 modulate 0x40000, else blend 0x30000) at the key's value. Keys {from, to, ticks}, a key of -1 ticks holds;
 * when they run out the effect stops and the stage is removed */
typedef struct OrreWzxEnvMap {
    int mode, type, count;
    struct { float from, to; int ticks; } key[16];
} OrreWzxEnvMap;
int orre_wzx_envmap(const OrreWzx* wzx, int index, OrreWzxEnvMap* out);
/* The screen warp (screen effect kind 12, distortionEffectCreate; tracefx.c case 12, distortionEffectStart,
 * _distortionUpdate, _distortionEffectUpdateMatrices, _distortionRender; verified from the code): a camera-facing square
 * at the owner's centre point (its model record's +0x54 part) that redraws the scene, re-rendered into a screen-sized
 * texture (its models; also its particles when `particles`), offset per pixel by the warp picture through GX indirect
 * texturing, with no depth test and no blending. Its half-size is size x frames, grown over the first `peak` of the
 * effect's time and shrunk over the rest (peak 1: grown all the way, or 0: shrunk from the start); the offsets' strength
 * runs from `strength` down to 0 */
typedef struct OrreWzxWarp {
    int particles;   /* +0x00: the capture also draws the particles (both links) */
    int follow;      /* +0x04: the place follows the part every update (else read once at the start) */
    float size;      /* +0x08: the half-size per tick of the effect's length */
    float strength;  /* +0x0C */
    float peak;      /* the growth's end as a share of the time: mode +0x14 1 -> 1.0, 2 -> the float at +0x18 */
    int frames;      /* the effect's length (ticks) */
    int tex_width, tex_height;
    const uint8_t* tex_rgba; /* the warp picture (owned by the WZX; NULL when it didn't decode) */
} OrreWzxWarp;
int orre_wzx_warp(const OrreWzx* wzx, int index, OrreWzxWarp* out);
/* its value `ticks` after the start (_envMapUpdate); 0 once the keys have run out */
int orre_wzx_envmap_value(const OrreWzxEnvMap* e, int ticks, float* value);
typedef struct OrreLeaves OrreLeaves;
/* origin: the owner's attach point in the world; effect_scale: the owner's effect scale */
OrreLeaves* orre_leaves_create(const OrreWzxLeaves* params, const float origin[3], float effect_scale, uint32_t seed);
void orre_leaves_free(OrreLeaves* leaves);
int orre_leaves_update(OrreLeaves* leaves, int ticks); /* one battle frame (2 ticks); 0 once ended */
/* the leaves still flying: each one's transform (3x3 rotation x scale, row-major, then the position: 12 floats) into
 * xf (room for 32), and the leaves' alpha; returns how many */
int orre_leaves_get(const OrreLeaves* leaves, float* xf, float* alpha);
int orre_wzx_billboard(const OrreWzx* wzx, int index, OrreWzxBillboard* out);
/* a particle entry's bank (its own, or the earlier entry's it links to); NULL otherwise. Owned by the WZX. */
const struct OrreParticleBank* orre_wzx_bank(const OrreWzx* wzx, int index);
/* The start of every entry in 60 Hz ticks from the sequence's start (wazaSequenceEntryLink, then shifted so the
 * earliest is 0), given the owner's time points for the WZX's slot (60 Hz, OrreModel.slot_times). Returns the count;
 * also the entries in the game's run order (sorted by start, a control step before others at the same tick) in
 * order[] when given. */
int orre_wzx_schedule(const OrreWzx* wzx, const int time_points[16], int* starts, int* order);
/* The ticks every start was moved later by (entries before the owner's time point 0; 0 for most): the owner's slot
 * animation reaches its first frame that late, playing its last lead / 2 frames before (wazaSequencePokemonMotionStart) */
int orre_wzx_lead_in(const OrreWzx* wzx, const int time_points[16]);
/* How long a sequence's effects run, in 60 Hz ticks from its start: every entry run dry until it has ended as
 * wazaSequenceEntryUpdate decides (particles: no generators and no particles left; colour fades: their keys run out;
 * models: their animation's end), capped at max_ticks (an emitter that never stops). Sounds aren't counted yet. The
 * owner's own animation is the caller's to add. Starts as orre_wzx_schedule gives them. */
int orre_wzx_run_ticks(const OrreWzx* wzx, const int time_points[16], int max_ticks);
/* wazaSequenceGetEffectScale: the effect scale for a size class (-2..3) and the move / part, as the game picks it */
float orre_wzx_effect_scale(int size_class, int move, int part);

/* ---- The battle's procedural move camera (battle_camera.cpp, 2026-11-19, release step 4) ----
 * The shot the battle builds for a move without its own camera: a motion picked at random (0 dolly, 1 slide, 2 orbit,
 * 3 held, 4 wide, 5 close-up) within the move's allowed set, distance / height / angle ranges fitted to the filmed
 * Pokémon, and a zoom from weighted timing patterns (docs/engine/battle-camera.md; src/game/wazaSequenceCamera.c).
 * Positions are in the battle's space. */
typedef struct OrreCamOwner {
    float pos[3];            /* the model's place */
    float rot_y;             /* its turn about y (radians) */
    float box_min[3], box_max[3]; /* its bounding box, in its own space */
    int size_class;          /* PKX header +0x0C */
    int facing;              /* +1 the near side, -1 the far side (mirrors the shot) */
    int is_pokemon;          /* 0 for a trainer (every duration doubled) */
    int partners;            /* a trainer's Pokémon out on the field (narrows its angle range); 0 for a Pokémon */
    int species;             /* the owner's index (0x13A always gets the wide shot) */
    int slot;                /* the playing slot (8 / 9 take the long pattern table) */
    int tp_count, tp[16];    /* that slot's time points (60 Hz) */
    float aim[3];            /* the aimed joint's place at the start */
} OrreCamOwner;
typedef struct OrreBattleCamera OrreBattleCamera;
OrreBattleCamera* orre_bcam_create(uint32_t seed);
void orre_bcam_free(OrreBattleCamera* cam);
void orre_bcam_reset(OrreBattleCamera* cam, uint32_t seed); /* also forgets the last motion */
/* starts a shot (battleCameraStartWaza): has_seq and the WZX header's flags (orre_wzx_header); returns the motion */
int orre_bcam_start(OrreBattleCamera* cam, const OrreCamOwner* owner, int has_seq, uint32_t flags);
/* one battle frame: step = retraces it took (2 at 30 fps); aim = the aimed joint's place now (NULL: unchanged) */
void orre_bcam_update(OrreBattleCamera* cam, int step, const float* aim);
/* the camera now: eye, interest (battle space), vertical field of view in degrees; 0 before any shot */
int orre_bcam_get(const OrreBattleCamera* cam, float eye[3], float interest[3], float* fov_deg);
/* For the exact check against the game's ORRE_BCAM_LOG (orrestudio-cli bcamtest): the previous shot's motion
 * (gLastMotionType), a frame that runs only the camera's own update (the log's "part=-" frames), and the shot's
 * picked motion, 12 parameters (size, height, rotation min / max / base, height min / max, distance min / max, near,
 * mid, far), zoom keys (start, end, start frame, end frame, twice) and the generator's state */
void orre_bcam_set_last_motion(OrreBattleCamera* cam, int motion);
void orre_bcam_set_start_step(OrreBattleCamera* cam, int step); /* the start's own update's frame step (2) */
/* the frame order: 1 a battle's (the camera updates before the sequences pick the aim: it lags a frame; the default),
 * 0 the move viewer's (the aim's step at once). Kept across resets */
void orre_bcam_set_order(OrreBattleCamera* cam, int battle);
void orre_bcam_update_camera_only(OrreBattleCamera* cam, int step);
/* the other half: only the sequence's aim (wazaSequenceCameraUpdateWaza), the shot not stepped. A battle's first
 * frame of a shot: the camera was started by the sequences after cameraUpdate ran, so its first step comes a frame
 * later (every shot of the project's P01 replay log: the eye holds on the first frame, also when a target is logged) */
void orre_bcam_update_aim_only(OrreBattleCamera* cam, int step, const float* aim);
int orre_bcam_debug(const OrreBattleCamera* cam, float params[12], float keys[8], uint32_t* seed);

/* ---- text ---- */

/* A message's text in UTF-8, cleaned for display (control codes removed; a line break code becomes a space), or "" if
 * the id is unknown. The pointer stays valid until the disc is closed. */
const char* orre_text(OrreDisc* disc, uint32_t msg_id);
/* the same, raw: control codes kept as <XX> or <XX:hex args> (the Python tools' notation) */
const char* orre_text_raw(OrreDisc* disc, uint32_t msg_id);
/* number of distinct message ids read from every bank on the disc (forces the full read; first call ~1 s) */
int orre_text_count(OrreDisc* disc);

/* ---- pictures ---- */

typedef struct OrreImage {
    int width, height;
    uint8_t* rgba; /* width * height * 4 bytes, straight (not premultiplied) alpha; free with orre_image_free */
} OrreImage;
void orre_image_free(OrreImage* img);

/* A sound as 16-bit mono PCM (2026-11-22, for the cries): free with orre_sound_free */
typedef struct OrreSound {
    int rate;       /* samples a second, as the game plays it */
    int count;      /* samples */
    int16_t* pcm;
} OrreSound;
void orre_sound_free(OrreSound* s);
/* A species' cry, as the game plays it (cry.cpp; docs/engine/music.md "Cries"): species record +0x0E (its sound id,
 * National Dex number + 569) -> the sound table in common_rel (+0x04: the MusyX effect id) -> that effect in the cry
 * banks (se_nakigoe: effects 0-200, se_nakigoe2: 980-1166) -> its macro -> the sample it starts, DSP-ADPCM decoded, at
 * the rate the effect's note gives it (all 386: note 60 on a sample keyed 60, so its own 11,025 Hz). 1 on success. */
int orre_species_cry(OrreDisc* disc, int species, OrreSound* out);

/* A species' mug shot (42 x 42), shiny 0/1, cell 0 = as the game's menus show it, 1 = the same face turned the other
 * way, where the sheet has that cell (31 species: a separately drawn face, not a mirror; verified by Main on every
 * sheet, 2026-11-18, textures.md). 1 on success; 0 when the species has no such picture. Which screens draw the
 * turned cell is open. (*Corrected 2026-11-18:* Studio briefly read the two cells as halves of one picture; Bulbasaur's
 * and Ivysaur's only look continuous because both three-quarter views face the join.) */
int orre_species_mug_shot(OrreDisc* disc, int species, int shiny, int cell, OrreImage* out);
/* A species' PC picture (the 64 x 64 body picture the PC box and the summary use), shiny 0/1. 1 on success. */
int orre_species_pc_picture(OrreDisc* disc, int species, int shiny, OrreImage* out);

/* ---- forms ----
 * How many looks a species has on this disc (verified from the code, 2026-11-18): Unown (species 201) has one per
 * letter, its count read from the game (lbl_80478CF0: 28 = A..Z, !, ?), each with its own mug shot
 * (lbl_8035B478[form][shiny], gs_range_80109C88.c), PC picture (lbl_802676F0[form][shiny]) and model
 * (lbl_80374E40[form], sequence.c). Every other species has 1. (Castform's weather looks are their own species-table
 * rows in Colosseum, pokemon-data.md; Deoxys has one model on this disc.) The functions without "form" use form 0. */
int orre_species_form_count(OrreDisc* disc, int species);
/* the model id a form is known by (the per-Pokémon move-effect table's key): Castform's weather forms their own
 * entries, every other form the species' own */
uint32_t orre_species_form_model_id(OrreDisc* disc, int species, int form);
/* The name of the archive a form's model comes from (pkx_unknown_a, pkx_powalen_f, ...): how the window labels forms
 * (Castform: 4 forms, its weather models ordered _f, _w, _i; it has no separate pictures for them, so the mug shot and
 * PC picture calls give its usual ones). 1 on success. */
int orre_species_form_archive(OrreDisc* disc, int species, int form, char* out, int size);
int orre_species_form_mug_shot(OrreDisc* disc, int species, int form, int shiny, int cell, OrreImage* out);
int orre_species_form_pc_picture(OrreDisc* disc, int species, int form, int shiny, OrreImage* out);

/* ---- 3D models ---- */

typedef struct OrreVertex {
    float pos[3];
    float nrm[3];      /* zero when the mesh has no normals */
    float uv[2];       /* the raw texture coordinate (GX_VA_TEX0); each stage applies its own transform */
    float clr[4];      /* vertex colour 0 (GX_VA_CLR0), RGBA 0..1; white when the mesh has none */
    float uv1[2];      /* the second and third raw coordinate sets (GX_VA_TEX1 / TEX2), zero when absent: layered */
    float uv2[2];      /* faces map their eye and mask layers with their own sets (Nuzleaf, Kirlia; 2026-11-18) */
    float uv3[2];      /* GX_VA_TEX3: the fourth set (Spheal's and Spinda's spots) */
    float uv4[2];      /* GX_VA_TEX4: the fifth set, the most any Pokémon uses (Spinda's mouth strip; 2026-11-18) */
} OrreVertex;

/* One texture stage of a material (HSD_TObjDesc, include/hsd/hsd_tobj.h; how stages combine: docs/engine/hsd-models.md
 * "Texture coordinates and texture stages", verified from src/hsd/tobj.c). Stages are in the material's list order,
 * only those whose picture decodes (as colo/hsdmodel.py counts them). */
typedef struct OrreTexStage {
    int texture;          /* index into OrreModel.textures */
    uint32_t flags;       /* TObjDesc +0x40: bits 0-3 coordinate source (TEX_COORD_*), 4-8 lighting channel
                             (diffuse, specular, ambient, ext, shadow), 16-19 colour op, 20-23 alpha op, 24 bump */
    float blending;       /* +0x44: the "blend" operation's factor */
    uint8_t wrap_s, wrap_t; /* +0x34 / +0x38: 0 clamp, 1 repeat, 2 mirror (GXTexWrapMode) */
    float su, sv, tu, tv; /* MakeTextureMtx without rotation: uv' = ((u + tu) * su, (v + tv) * sv) */
    int rotated;          /* the stage has a rotation (not applied; none found on Pokémon so far) */
    int has_tev;          /* +0x58 TEV override present: tev[] holds it */
    uint32_t map_id;      /* +0x08 GXTexMapID: how a material animation's texture animation finds this stage */
    float translate[2];   /* +0x28 / +0x2C and +0x1C / +0x20: the raw values su..tv are made from, so a texture */
    float scale[2];       /* animation's scroll (TRAU / TRAV) or scale (SCAU / SCAV) can rebuild them */
    uint8_t repeat[2];    /* +0x3C / +0x3D */
    int uv_set;           /* +0x0C GXTexGenSrc: which coordinate set the stage reads, 0..2 for GX_TG_TEX0..TEX2 (4..6);
                             -1 for any other source (position, normal: unused on Pokémon so far) */
    uint8_t tev[32];      /* the TEV override as stored (HSD_TObjTevDesc, include/hsd/hsd_tobj.h): colour op, alpha op,
                             biases, scales, clamps, colour inputs a..d (+8), alpha inputs a..d (+0xC), konst RGBA
                             (+0x10), registers tev0 / tev1 (+0x14 / +0x18), u32 active (+0x1C: bit 30 colour, 31 alpha).
                             MakeColorGenTExp (src/hsd/tobj.c) turns it into a stage that replaces the texture's colour
                             and / or alpha before the stage's usual operation (Roselia's roses get their red and blue
                             from it). Zero when has_tev is 0. */
} OrreTexStage;

/* GX's own limit. *Updated 2026-11-18:* was 4, which dropped the fifth stage of Spinda's and Deoxys's materials
 * (the most any Pokémon uses; survey). */
#define ORRE_MAX_STAGES 8

typedef struct OrreMesh {
    int texture;          /* the first stage's texture, or -1 (kept for simple viewers) */
    uint8_t diffuse[4];   /* material colours (HSD_Material +0x04, +0x00) */
    uint8_t ambient[4];
    float alpha;          /* HSD_Material +0x0C */
    uint32_t render_mode; /* HSD_MObjDesc +0x04: RENDER_* flags (include/hsd/hsd_mobj.h) */
    int has_pe;           /* the material has its own pixel-engine settings (HSD_MObjDesc +0x14) */
    uint8_t pe[12];       /* HSD_PEDesc as stored: flags, ref0, ref1, dst_alpha, type, src, dst, logic, z_comp,
                             alpha_comp0, alpha_op, alpha_comp1 */
    int cull;             /* from the PObj flags: 0 none, 1 front, 2 back, 3 both (not drawn) */
    int has_colours;      /* the mesh carries vertex colours */
    int stage_count;
    OrreTexStage stages[ORRE_MAX_STAGES];
    int hidden;           /* its own joint is flagged hidden in the standing pose (an animation may show it) */
    int vertex_count;     /* a multiple of 3 (triangles) */
    OrreVertex* vertices;
    uint8_t specular[4];  /* HSD_Material +0x08: the highlight colour where the render mode has RENDER_SPECULAR (bit 3) */
    float shininess;      /* HSD_Material +0x10: how tight the highlight is (Grumpig's black pearls) */
    /* A glow card (2026-11-19): a part on a joint that always faces the camera (a billboard), drawn see-through
     * without writing depth (RENDER_XLU and RENDER_NO_ZUPDATE), such as the sparkle on Espeon's forehead gem. Not
     * one of the game's particles, but Studio's Particles switch hides it with them (snatcherLeo, 2026-11-19: "can we make it
     * so that, the sparkle effect can be considered a particle ... so it can be turned off? Similar to Vaporeon's") */
    int glow;
} OrreMesh;

typedef struct OrreModel {
    int mesh_count;
    OrreMesh* meshes;
    int texture_count;
    OrreImage* textures;
    int joint_count;
    float bounds_min[3], bounds_max[3]; /* over the visible meshes */
    int animation_count;  /* joint animations of the model set (its list at model set +0x04) */
    int idle_animation;   /* the animation the PKX's slot 0 (idle) plays, or -1 (orre_species_model fills it) */
    void* internal;       /* the core's own data for posing (skeleton, tracks, skin weights); freed by orre_model_free */
    int particle_bank_size; /* bytes of the PKX's particle bank (GPT1) beside the model, 0 when it has none */
    /* The PKX's animation slots (2026-11-19, the Moves tab): what each slot plays, its first {type 0, value} pair
     * (docs/engine/models-animation.md "Animation slot record"); -1 = no animation. Slots: 0 idle, 1 ranged attack,
     * 2 body attack, 3 strike, 4 kick, 5 take-off, 6 dive, 7 peck, 8 hit, 9 hit + second motion, 10 faint, 11 idle 2,
     * 12-15 its own moves, 16 idle-like (names: likely, animation-catalog.md). Slots the model lacks fall back as the
     * game's do: 11 -> 0, 12-15 -> 1 (orre_model_slot_animation). */
    int slot_count;
    int slot_animation[17];
    /* the PKX's own particle bank (its ambient effects: Charizard's tail flame), or NULL; freed by orre_model_free */
    struct OrreParticleBank* particles;
    /* The shiny recolour (filled when the model was read as shiny and its PKX has a colour block; zero otherwise):
     * shiny_swap = which source channel feeds R, G, B, A (0..3), applied to every texture and raster colour read when
     * shiny_swap_on; shiny_mod = RGBA modulation (0x7F = x1: the colour becomes 2 x mod x colour) when shiny_mod_on.
     * The game's own path for the 378 species without a separate shiny model (textures.md "Runtime recolour"). */
    int shiny_swap_on, shiny_mod_on;
    uint8_t shiny_swap[4], shiny_mod[4];
    /* The eyes' texture animations from the PKX header (0x18, 0x1C, 0x20; verified, the attach-points research):
     * [0] eyes closing (sleep), [1] eyes opening (waking), [2] the idle blink; -1 = none. Indices into the model's
     * texture animations, which run parallel to its animations. Filled by orre_species_model; -1 otherwise. */
    int eye_texanim[3];
    /* What move effects need from the PKX (2026-11-19, release step 4; models-animation.md "Animation slot record",
     * studio/attach-points.md; verified from the loader and wazaSequenceEntryLink / wazaSequenceEntryGetAttachPart):
     * size_class = header +0x0C (-2..3: scales every move effect x0.5 / 0.75 / 1 / 1.333 / 2 / 3.25);
     * slot_times[slot] = the slot's 16 time points as stored (60 Hz frames; the game scales them by the video rate /
     * 60, so 30 fps battle frames are half) - a move's entries start relative to them;
     * slot_points[slot] = the slot's 16 attach points (joints in the walk order; -1 none). Zero when not a PKX. */
    int size_class;
    int slot_times[17][16];
    int slot_points[17][16];
    int slot_time_count[17];  /* the record's +0x00: how many time points it uses (the camera's shot length and zoom) */
    int model_id;             /* the species' model id (pkxDataId) it was read for; 0 when not a PKX */
} OrreModel;
/* the animation a slot plays on this model, with the game's fallbacks (11 -> 0, 12-15 -> 1); -1 if none */
int orre_model_slot_animation(const OrreModel* model, int slot);

/* What a model uses beyond plain textured meshes (for the survey: `orrestudio-cli survey`). Joint flags are
 * include/sysdolphin/baselib/jobj.h's; material and texture-stage bits are hsd_mobj.h's and hsd_tobj.h's. */
typedef struct OrreModelSurvey {
    int billboard_joints;   /* joints flagged to turn toward the camera (JOBJ_BILLBOARD_FIELD 0xE00 or PBILLBOARD) */
    int particle_joints;    /* joints that carry a particle generator instead of display objects (JOBJ_PTCL) */
    int spline_joints;      /* joints that hold a spline (JOBJ_SPLINE) */
    int xlu_joints, texedge_joints; /* joints in the translucent and cut-out drawing passes (JOBJ_XLU, JOBJ_TEXEDGE) */
    int colour_anims;       /* material animations that animate colours (HSD_MatAnim +0x04 set) */
    int render_anims;       /* material animations with a render animation (HSD_MatAnim +0x0C set) */
    int tev_stages;         /* texture stages with a TEV override (TObjDesc +0x58) */
    int dropped_stages;     /* decodable stages past ORRE_MAX_STAGES (not drawn) */
    int max_uv_set;         /* the highest coordinate set a stage reads (0 = GX_TG_TEX0) */
} OrreModelSurvey;
int orre_model_survey(const OrreModel* model, OrreModelSurvey* out);

/* Where a model's texture comes from (2026-11-19, the all-models check): its image's pixel data and its palette as
 * offsets into the model archive's data block (the file after its 0x20-byte header, where HSD's pointers point), the
 * GX texture format, the palette's format (GXTlutFmt; 0 when there is none) and the size. Lets Studio's list of what
 * each part draws be compared with what the game binds, exactly. 1 on success. */
typedef struct OrreTextureSource {
    uint32_t data_offset, format, tlut_offset, tlut_format;
    int width, height;
} OrreTextureSource;
int orre_model_texture_source(const OrreModel* model, int texture, OrreTextureSource* out);
/* Which joint a mesh hangs from (numbered in the depth-first order the game's part index uses) and which of that
 * joint's display objects (DObjs, in list order) it belongs to. 1 on success. */
int orre_model_mesh_owner(const OrreModel* model, int mesh, int* joint, int* dobj);

/* ---- Particle banks (GPT1; particles.cpp, 2026-11-19) ----
 * HAL's HSD particle system (docs/engine/particles.md): a bank holds emitter programs (a 0x3C-byte header, then the
 * particle byte-code) and texture groups (images in GX formats). The same format serves the Pokémon's own effects
 * (a PKX's bank: Charizard's tail flame), the moves' effects (WZX files) and rooms. Read-only here; the simulation
 * that plays them is below. */
typedef struct OrreParticleBank OrreParticleBank;
/* a GPT1 bank (its bytes are copied); NULL when it isn't one */
OrreParticleBank* orre_particle_bank_read(const uint8_t* data, size_t size);
void orre_particle_bank_free(OrreParticleBank* bank);
/* one emitter program (HSD_PSCmdList): its header as stored, and its byte-code's size */
typedef struct OrreParticleEmitter {
    int index, id;     /* its place in the bank, and id = the bank's first id + index */
    int present;       /* 0: the bank's table has no program at this place */
    uint16_t type, tex_group, gen_life, life;
    uint32_t kind;
    float gravity, friction, vel[3], radius, angle, random, size, param[3];
    int code_size;
} OrreParticleEmitter;
int orre_particle_bank_count(const OrreParticleBank* bank, int* first_id); /* emitter places (and the first id) */
int orre_particle_emitter(const OrreParticleBank* bank, int index, OrreParticleEmitter* out);
const uint8_t* orre_particle_emitter_code(const OrreParticleBank* bank, int index); /* its byte-code (code_size bytes) */
/* texture groups: the count; one group's GX format, size and image count; one decoded image (RGBA8; NULL if not
 * decodable). The images belong to the bank. */
int orre_particle_group_count(const OrreParticleBank* bank);
int orre_particle_group(const OrreParticleBank* bank, int group, int* gx_format, int* width, int* height, int* images);
const OrreImage* orre_particle_group_image(const OrreParticleBank* bank, int group, int image);
/* the id table (one u32 per emitter: what the "id table" byte-code commands look up) */
uint32_t orre_particle_bank_ref(const OrreParticleBank* bank, int index);

/* ---- The particle simulation (particles.cpp, 2026-11-19): HSD's particle system ported line by line ----
 * (psInterpretParticle0 / psInterpretParticles: src/game/psinterpret.c; psCreateGeneratorID, psExecGenerator and
 * generateParticle_8017424C: src/game/generator.c; psGenerateParticle0: src/game/particle.c; the list helpers:
 * src/game/pslist.c; the glue gsParticleCreate / gsParticleUpdateAll: src/game/field_range_80117E58.c.) One step = one
 * 60 Hz retrace; the battle runs 2 per 30 fps frame. Random numbers: the game's own generator (HSD_Randf: an LCG,
 * state * 214013 + 2531011, (state >> 16) / 65536), one per system, so a seed reproduces a run. Not yet: app SRTs,
 * texture forms and the move effects' field-node transforms (moves, step 2). */
typedef struct OrreParticleSystem OrreParticleSystem;
OrreParticleSystem* orre_particles_create(void);
void orre_particles_free(OrreParticleSystem* sys);
/* kills every particle and generator and reseeds the random generator (the banks stay) */
void orre_particles_reset(OrreParticleSystem* sys, uint32_t seed);
/* the bank in slot 0..63 (not owned; NULL clears) */
void orre_particles_set_bank(OrreParticleSystem* sys, int slot, const OrreParticleBank* bank);
/* gsParticleCreate: starts emitter `emitter_id` (the bank's absolute id, as a particle key names it) of bank `slot` on
 * link 0; returns a handle (> 0) or 0. The generator keeps its particles until they die; a finished one is reclaimed
 * by the step once its last particle is gone (gsParticleUpdateAll). */
int orre_particles_start(OrreParticleSystem* sys, int slot, int emitter_id);
/* the same on a draw link (link 1: a WZX particle entry's mode bit 0) */
int orre_particles_start_link(OrreParticleSystem* sys, int slot, int emitter_id, int link);
/* The move effects' particle node (2026-11-19, release step 4; field_range_80117E58.c, verified from the code):
 * node_flags: the WZX entry's mode word applied to the generator (orre_wzx_entry "mode");
 * node_set: what 0 position / 1 rotation / 2 scale (Euler radians), straight to the generator while unlinked, the
 *   offset while linked; for the scale, apply = 1 also scales the emitter's shape (psSetGeneratorAngleRadiusScale,
 *   motion = 1 gravity and friction too);
 * node_position: the generator's position when it was created;
 * node_link: gsParticleLinkToJObj: state 1-7 = what follows the joint (1 position, 2 rotation, 3 scale, 4 pos + rot,
 *   5 rot + scale, 6 pos + scale, 7 all), once = only the first follow, from_zero = the offset starts at zero (else
 *   the values set so far), link_jobj = generators its particles start follow the joint too;
 * node_due: a follow is due this frame; node_follow: the joint's world position, rotation (the sum of the Euler angles
 *   along its chain) and scale (their product), given before each frame's steps;
 * node_alive: still running or with particles alive. */
void orre_particles_node_flags(OrreParticleSystem* sys, int handle, uint32_t mode);
void orre_particles_node_set(OrreParticleSystem* sys, int handle, int what, const float v[3], int apply, int motion);
void orre_particles_node_position(OrreParticleSystem* sys, int handle, float out[3]);
void orre_particles_node_link(OrreParticleSystem* sys, int handle, int state, int once, int from_zero, int link_jobj);
int orre_particles_node_due(const OrreParticleSystem* sys, int handle);
void orre_particles_node_follow(OrreParticleSystem* sys, int handle, const float pos[3], const float rot[3], const float scale[3]);
int orre_particles_node_alive(OrreParticleSystem* sys, int handle);
/* gsParticleUpdate's follow, attach mode 1 (a Pokémon's: position only): the generator's position, set before stepping */
void orre_particles_set_position(OrreParticleSystem* sys, int handle, const float pos[3]);
/* a point joint (1..8: what the 0x2A keys set; B7 / B8 aim at it) at a world position; NULL clears it */
void orre_particles_set_point(OrreParticleSystem* sys, int no, const float* pos);
/* one 60 Hz step: every particle runs (psInterpretParticles), then every generator (psExecGenerator), then finished
 * generators are reclaimed */
void orre_particles_step(OrreParticleSystem* sys);
/* gsParticleUpdateAll: reclaims the started emitters that have finished and whose particles are all gone. The game
 * runs it once per 30 fps frame, after the animation's keys started new emitters and before the two steps */
void orre_particles_reclaim(OrreParticleSystem* sys);
int orre_particles_count(const OrreParticleSystem* sys); /* live particles */
int orre_particles_generators(const OrreParticleSystem* sys);

/* One particle to draw, as psDispMain / psDispSub build it: four corners in world space (bottom-left, top-left,
 * top-right, bottom-right on screen), each with a texture coordinate and a colour (the same colour on all four except
 * a trail's tail), plus the state psDispMain sets for it. Listed in the game's drawing order (sort groups, then the
 * list's order: newest first). */
typedef struct OrreParticleQuad {
    float pos[4][3];
    float uv[4][2];
    uint8_t colour[4][4];   /* the rasterised colour: prim, or the trail's per-corner colour (psSetupTev's "ras") */
    uint8_t prim[4], env[4], mat[4]; /* TEV C0 / C1 / C2 (env forced to 0 unless PrimEnv; psdisp setupTevReg) */
    uint32_t kind;          /* the particle's kind bits (TEV key = kind & 0x80100480; blend = bits 22-23) */
    int textured, bank, group, image;
    uint8_t alpha_mode, alpha_ref0, alpha_ref1; /* GX alpha compare: comp0 = (mode >> 3) & 7, op = mode >> 6, comp1 = mode & 7 */
    uint8_t link;           /* its generator's link: 1 draws in the second pass, after the depth reset (GSgfxDrawScene's
                             * psDispMain link masks: pass 0 link 0, pass 1 link 1; verified by the project's capture) */
} OrreParticleQuad;
/* view: the world-to-view matrix, 12 floats (3 rows of 4); proj_perspective: 1 for a perspective camera; proj_xx /
 * proj_xz / proj_yy / proj_yz: the projection's entries GXGetProjectionv gives (for the screen-space direction of
 * trails). Returns how many quads (may exceed max). */
int orre_particles_quads(const OrreParticleSystem* sys, const float view[12], const float proj[4], OrreParticleQuad* out, int max);

/* One particle key that fires while an animation plays (HSD track type 0x28): the animation frame, the joint it
 * fires at, and the bank index and emitter number it packs (the game's gsParticlePtclHook starts that emitter of the
 * model's linked GPT1 bank there). */
typedef struct OrreParticleKey { int frame, joint, bank, emitter; } OrreParticleKey;
/* Plays `animation` once from frame 0 and lists its particle keys in firing order per joint (at most `max` written).
 * Returns how many fired (may exceed max). */
int orre_model_particle_keys(const OrreModel* model, int animation, OrreParticleKey* out, int max);

/* An animation's length in frames (its AObjDesc end frame), or 0 */
float orre_model_animation_length(const OrreModel* model, int animation);
/* Poses the model: animation -1 = the bind (standing) pose, else that animation at `frame` (0 .. length; the caller
 * loops). Writes each mesh's posed vertices into out[mesh] (vertex_count entries each; uv and colour copied).
 * Keyframes are interpreted exactly as HSD's FObj player does (src/hsd/fobj.c). 1 on success. */
int orre_model_pose(const OrreModel* model, int animation, float frame, OrreVertex* const* out);
/* The same, for a camera: view = the world-to-view matrix as 12 floats (3 rows of 4, row-major). Parts on billboard
 * joints (JOBJ_BILLBOARD / V / H / R, 18 species: Jumpluff's puffs, Chinchou's and Lanturn's glows, Espeon's gem) are
 * turned to face it as HSD_JObjMakePositionMtx does (src/hsd/displayfunc.c). view NULL = orre_model_pose. */
int orre_model_pose_view(const OrreModel* model, int animation, float frame, const float* view, OrreVertex* const* out);
/* Each joint's local values at a frame: 9 floats per joint (rotation X Y Z in radians, scale X Y Z, translation
 * X Y Z), the joint's own values where no track drives a channel; joints in the model's walk order. For the golden test
 * and, later, an animation editor's channel view. 1 on success. */
int orre_model_joint_values(const OrreModel* model, int animation, float frame, float* out);
/* GSpartGetTransform's rotation and scale for one joint at a frame: the Euler rotations along its chain to the root
 * added, the scales multiplied (what a move effect following the joint takes; its position is the joint's world
 * translation, orre_model_joint_world). 1 on success. */
int orre_model_part_chain(const OrreModel* model, int animation, float frame, int joint, float rot[3], float scale[3]);
/* Each joint's world matrix at a frame, as the posing computes it (IK and constraints applied): 12 floats per joint
 * (3 rows of 4, row-major), joints in the walk order; scl3 (may be NULL) gets each joint's accumulated scale
 * (jobj->scl, 0 0 0 where it has none). For comparing with the game's own matrices. 1 on success. */
int orre_model_joint_world(const OrreModel* model, int animation, float frame, float* out12, float* scl3);
/* Which mesh parts are drawn at a frame (animation -1: the standing pose): shown[mesh] = 1 / 0. A joint's hidden flag
 * hides only its own parts; animations switch it per frame (HSD_A_J_NODE / BRANCH tracks), e.g. eye states. */
int orre_model_visibility(const OrreModel* model, int animation, float frame, uint8_t* shown);
/* Each texture stage's picture and blend factor at a frame (texture animations: HSD_TexAnim, run by the same FObj
 * player; TObjUpdateFunc, src/hsd/tobj.c): texture[mesh * ORRE_MAX_STAGES + stage] = index into OrreModel.textures,
 * blend[...] = the stage's blend factor (0..1). Stages no animation touches keep their own values. Eyes and mouths
 * often change this way (Shellder's eyes, Pikachu's face). 1 on success. */
int orre_model_texture_frame(const OrreModel* model, int animation, float frame, int* texture, float* blend);
/* The same, plus each stage's coordinate transform at the frame: xform[(mesh * ORRE_MAX_STAGES + stage) * 4 + 0..3]
 * = su, sv, tu, tv (as OrreTexStage). Texture animations scroll and scale coordinates (TRAU / TRAV / SCAU / SCAV):
 * many Pokémon pick their eye or mouth state by scrolling a strip of pictures (Shellder, Mankey; verified). */
int orre_model_texture_frame2(const OrreModel* model, int animation, float frame, int* texture, float* blend, float* xform);
/* Battle animations advance 0.5 animation frames per game frame (the battle's rate constant, verified from the US
 * disc) in scenes that run at 30 frames a second, drawn at whole frames: 30 animation frames per second (likely). */
#define ORRE_BATTLE_ANIM_FPS 30.0f

/* Material colours at a frame: per mesh ORRE_MATERIAL_FLOATS floats, 0..1: diffuse RGB + material alpha (0..3),
 * ambient RGBA (4..7), then per stage (ORRE_MAX_STAGES x 12) the TEV override's konst RGBA, tev0 RGBA, tev1 RGBA. Colour animations
 * (HSD_MatAnim +0x04; 5 species: Gastly, Chinchou, Lanturn, Magcargo, Volbeat) and the texture animations' KONST / TEV0
 * / TEV1 tracks are applied; animation -1 or a mesh without them gives the material's own values. 1 on success. */
#define ORRE_MATERIAL_FLOATS (8 + 12 * ORRE_MAX_STAGES)
int orre_model_material_frame(const OrreModel* model, int animation, float frame, float* out);

/* A species' battle model in its bind pose (shiny 1: the separate shiny model where the game has one, else the
 * normal one). 1 on success; free with orre_model_free. */
int orre_species_model(OrreDisc* disc, int species, int shiny, OrreModel* out);
int orre_species_form_model(OrreDisc* disc, int species, int form, int shiny, OrreModel* out);
/* the same model read from another region's disc (`files`), found through `tables` (the US disc, whose main.dol tables
 * the core reads): a region's own style (Jynx's Japanese model). 1 on success; free with orre_model_free */
int orre_species_form_model_from(OrreDisc* tables, OrreDisc* files, int species, int form, int shiny, OrreModel* out);
void orre_model_free(OrreModel* model);

/* ---- raw access (CLI, tests) ---- */

/* the unpacked bytes of entry `entry` of archive `archive` ("common", without ".fsys"); free with orre_free */
uint8_t* orre_fsys_entry(OrreDisc* disc, const char* archive, int entry, size_t* size);
/* an HSD model archive straight from an archive entry (a map's DAT: every model set of its scene_data), 1 on success;
 * free with orre_model_free. The Moves preview's arena: waza_viewer entry 3, M1_water_colo.dat (the Phenac Colosseum) */
int orre_fsys_model(OrreDisc* disc, const char* archive, int entry, struct OrreModel* out);
void orre_free(void* p);

/* PNG file writer (no compression library: stored deflate blocks), for the CLI and the golden tests. 1 on success. */
int orre_png_write(const char* path, const OrreImage* img);

#ifdef __cplusplus
}
#endif
#endif
