"""Committed knowledge for extracted/ READMEs: for each kind of folder, how it was extracted, what it is
called / used for in-game, and insights about it (each marked verified or guess). extract.py renders
these into every folder's README.md, so a fresh extraction recreates them and nothing learned lives
only in the uncommitted extracted/ folder. Sources: docs/engine/*.md (keep both in step).
Patterns are matched against the folder path relative to extracted/ (glob style, '*' = one level)."""
import fnmatch

V, G = "verified", "guess"

# (pattern, {"how": [...], "ingame": [...], "insights": [...]}) - first match wins, most specific first.
NOTES = [
    ("*/particles*", {
        "how": [("extract.py (stages maps, effects, other): every GPT1 particle bank (resource types 10 and 18) as a readable listing "
                 "<name>.txt (each emitter's settings and byte-code) and <name>_textures/ (its texture groups as PNG); colo/gpt1.py", V)],
        "ingame": [("room effects (smoke, sparkles, water, explosions), the sleep / freeze status loops, menu and world-map effects", V),
                   ("which effect each bank is: from its name and room; not yet watched in the game", G)],
        "insights": [("the same HSD particle format as the Pokémon ambient banks and the banks inside move effects (WZX)", V),
                     ("an emitter's byte-code: waits, texture frames, colour / size / rotation ramps, child particles and generators (docs/engine/particles.md)", V)]}),
    ("pokemon/*/portraits", {
        "how": [("extract.py stage `pokemon`: species face entries -> menu-face records -> poke_face sheet, cropped to 42x42 cells (colo/gx.py)", V)],
        "ingame": [("the Pokémon's portrait (mugshot) in the party, summary, PC box, trade and battle menus", G)],
        "insights": [("one sheet per species: top cell normal, bottom cell shiny (hand-drawn, not a recolour); an 84-wide sheet adds an other-facing column (fn_8010B9E8)", V),
                     ("48 species have a separate other-facing portrait; the rest are presumably flipped (visual check 7)", G)]}),
    ("pokemon/*/body_pictures", {
        "how": [("extract.py stage `pokemon`: body ids in species face[0] / face[1] are entry ids in poke_body.fsys", V)],
        "ingame": [("64x64 body pictures (which screens use them is open)", G)],
        "insights": [("shiny body pictures are separate files (`_c` suffix = colour); the only Pokémon image with a separate shiny file besides the portraits' shiny cell", V)]}),
    ("pokemon/*/model_shiny*", {
        "how": [("extract.py stage `pokemon`: shiny-model table lbl_80374F20 in main.dol (species -> pkx_rare_* archive)", V)],
        "ingame": [("the battle model used when this Pokémon is shiny", V)],
        "insights": [("only 35 species have one; for them the runtime recolour is skipped (fn_801DE190)", V),
                     ("Charizard, Articuno and Moltres shiny models carry the same particle bank as the normal ones", V)]}),
    ("pokemon/*/model", {
        "how": [("model/<name>.obj + .mtl + <name>_obj_texNN.png: the model as OBJ in its bind pose (colo/hsdmodel.py, docs/engine/hsd-models.md); <name>_hidden.obj = meshes on hidden joints", V), ("extract.py stage `pokemon`: species pkxDataId -> Pokémon-model table lbl_80370BD0 in main.dol -> pkx_<name>.fsys", V)],
        "ingame": [("the battle model with its 17 animation slots (idle, attacks, damage, faint, ...)", V)],
        "insights": [("the .dat is the bare HSD model inside the PKX (offset 0x40), readable by the Blender GameCube importer", V),
                     ("12 species carry an ambient particle bank (_ambient_particles.gpt1), linked to the model at load (sequenceLoad)", V),
                     ("shiny colours for most species are a runtime channel swap + multiply stored in the PKX's last 20 bytes", V)]}),
    ("pokemon/*/textures", {
        "how": [("extract.py: image descriptors found in the HSD model by a heuristic scan (colo/hsd.py); base mip level only", V)],
        "ingame": [("the model's textures (skin, eyes, eye-blink strips, ...)", V)],
        "insights": [("originals are inside ../model/original/<name>.pkx (the textures are part of the model file)", V),
                     ("the scan may miss a texture or decode a false match", G)]}),
    ("pokemon/*", {
        "how": [("extract.py stage `pokemon`, readers colo/tables.py (species table), colo/gx.py, colo/pkx.py, colo/hsd.py", V)],
        "ingame": [("one Pokémon species, named from its species-name message", V)],
        "insights": [("folder number = the game's internal species index (Gen 3 order), not the National Dex; the species record has the National Dex number at +0x12 and the Hoenn number at +0x14", V)]}),
    ("pokemon", {
        "how": [("extract.py stage `pokemon` (python -I tools/local/extract.py <iso> pokemon)", V)],
        "ingame": [("all 386 species that have a name (index 0 and the 25 unused Gen 3 placeholders are skipped)", V)],
        "insights": [("internal order puts Hoenn Pokémon after a gap (indexes 252-276 unused), as in the GBA games", V)]}),
    ("trainers/character_art*", {
        "how": [("extract.py stage `trainers`: type-9 textures of chara_big.fsys / chara_small.fsys", V)],
        "ingame": [("character pictures (likely the large art in story screens and small icons)", G)],
        "insights": []}),
    ("trainers/*", {
        "how": [("model/<name>.obj + .mtl + <name>_obj_texNN.png: the model as OBJ in its bind pose (colo/hsdmodel.py, docs/engine/hsd-models.md); <name>_hidden.obj = meshes on hidden joints", V), ("extract.py stage `trainers`: PKX archives with a model suffix (_a1, _m_, _f_); class names from the trainer-model table lbl_80370840 when present", V)],
        "ingame": [("a trainer's or character's battle model (Wes = ken_a1, Rui = hizuki_a1)", V)],
        "insights": [("same PKX format and 17 slots as Pokémon, used differently (ball throws, commands); slot meanings in docs/engine/animation-catalog.md, visual check 5", G),
                     ("file names are romanized Japanese job names (ippan = ordinary person, kenkyuin = researcher, snach_zako = Snagem grunt)", V)]}),
    ("trainers", {"how": [("extract.py stage `trainers`", V)], "ingame": [("battle models of trainers and characters", V)], "insights": []}),
    ("npcs/*", {
        "how": [("model/<name>.obj + .mtl + <name>_obj_texNN.png: the model as OBJ in its bind pose (colo/hsdmodel.py, docs/engine/hsd-models.md); <name>_hidden.obj = meshes on hidden joints", V), ("extract.py stage `npcs`: type-2 models of people_archive.fsys and field_common.fsys", V)],
        "ingame": [("an overworld character or object model (walking NPCs, the player, Rui, treasure boxes)", V)],
        "insights": [("`_b1` / `_b2` suffixes are overworld variants; `_a1` names are the battle versions (trainers/)", G),
                     ("rooms place these through their per-room character tables (docs/engine/game-data-tables.md)", V)]}),
    ("npcs", {"how": [("extract.py stage `npcs`", V)], "ingame": [("overworld models of people and objects", V)], "insights": []}),
    ("maps/*/*/map*", {
        "how": [("model/<name>.obj + .mtl + <name>_obj_texNN.png: the model as OBJ in its bind pose (colo/hsdmodel.py, docs/engine/hsd-models.md); <name>_hidden.obj = meshes on hidden joints", V), ("extract.py stage `maps`: the room's type-1 entry (floorReadMap*) and its textures", V)],
        "ingame": [("the room's 3D map (the walkable scene)", V)],
        "insights": [("HSD map scene; the Blender importer reads it; map textures are extracted by the heuristic scan", V)]}),
    ("maps/*/*/cameras", {
        "how": [("extract.py stage `maps`: the room's type-12 entries (HSD archives with scene_data), summarised by colo/hsdcam.py", V)],
        "ingame": [("the room's scripted camera shots (cutscenes, intros); which event plays which shot is open", G)],
        "insights": [("cameras are plain HSD camera descriptors with keyframe tracks, the same format as Melee's stage cameras", V),
                     ("the room module's camera table (see the room README) lists the room's shots", G)]}),
    ("maps/*/*/collision", {
        "how": [("extract.py stage `maps`: the room's type-3 entry (CCD), parsed by colo/collision.py and written as Wavefront OBJ", V)],
        "ingame": [("what the player and NPCs walk on and bump into in this room; the game draws it only in a debug view (GScolsys2Draw)", V)],
        "insights": [("one shared triangle format for floors and walls; walkable floors face up, walls are vertical (from the normals)", V),
                     ("the player and NPCs have separate wall sets (+0x28 / +0x34), so NPCs can be kept out of places the player may go", V),
                     ("walk-through triggers and examine planes are collision triangles with an event id (+0x32), so doors, exits and signs are placed in the collision data, not the map model", V),
                     ("objects with flag bit 0 move (doors, lifts; guess for what they are) and are queried through their own matrix; static objects use a 40-unit grid for fast lookups", V),
                     ("floor triangles carry two attribute bytes (+0x30 / +0x31) that the layer query returns: likely floor layer and sub-layer (guess)", G)]}),
    ("maps/*/*/objects*", {
        "how": [("model/<name>.obj + .mtl + <name>_obj_texNN.png: the model as OBJ in its bind pose (colo/hsdmodel.py, docs/engine/hsd-models.md); <name>_hidden.obj = meshes on hidden joints", V), ("extract.py stage `maps`: the room's type-2 entries (floorReadObj*)", V)],
        "ingame": [("the NPCs and objects that appear in this room", V)],
        "insights": []}),
    ("maps/*/*", {
        "how": [("extract.py stage `maps`: every entry of the room archive; collision is exported to collision/ (OBJ); script, camera, particles and the room module are kept raw in original/", V)],
        "ingame": [("one room (floor); rooms ending _bf are the battle-field stages used for battles there", V)],
        "insights": [("each room has a small REL module that fills its NPC, position, camera and lighting tables when the room loads", V),
                     ("its script (original/*.scd) starts the room's music and events; readable listing in scripts/<area>/<room>__<script>.txt (extract.py stage `scripts`)", V)]}),
    ("maps/*", {
        "how": [("extract.py stage `maps`, grouped by the room-name prefix", V)],
        "ingame": [("one area (town or dungeon)", V)],
        "insights": [("area names verified by snatcherLeo except T1 (a stand-alone Colosseum arena); D3 is skipped on the disc (cut dungeon; reused for XD's S.S. Libra)", V)]}),
    ("maps", {"how": [("extract.py stage `maps`", V)], "ingame": [("every room of the game", V)], "insights": []}),
    ("ui/*", {
        "how": [("extract.py stage `ui`: type-9 textures of one menu / screen archive (colo/gx.py)", V)],
        "ingame": [("the screens of this menu (see the archive name; docs/engine/fsys-catalog.md, Menus)", G)],
        "insights": []}),
    ("ui", {"how": [("extract.py stage `ui`", V)], "ingame": [("menus, logos and screens", V)], "insights": []}),
    ("effects/moves/*/*_parts", {
        "how": [("extract.py stage `effects`: the data embedded in one WZX file: particle banks (particles/eNN_particles.txt + textures), models and the move's own archive (.dat), screen-effect textures (PNG); eNN = entry number in the timeline", V)],
        "ingame": [("the pieces one part of a move's animation is built from", V)], "insights": []}),
    ("effects/moves/*", {
        "how": [("extract.py stage `effects`: wzx_<move>_<part>.fsys -> <part>.txt (timeline) + <part>_parts/ (particle banks, models, textures); colo/wzx.py; originals in original/", V)],
        "ingame": [("the animation sequence of one move (attack part, damage part, ...)", V)],
        "insights": [("folder names are romanized Japanese move names (10manvolt = Thunderbolt, hinoko = Ember)", V),
                     ("each effect entry is pinned to a time point of the attacker's animation slot, so one file fits every species", V)]}),
    ("effects*", {"how": [("extract.py stage `effects`", V)], "ingame": [("move and battle effects", V)], "insights": []}),
    ("text", {
        "how": [("extract.py stage `text`: every type-5 message bank + the 3 banks inside common_rel (colo/msg.py)", V)],
        "ingame": [("all game text: dialogue, menus, names, descriptions", V)],
        "insights": [("text is UTF-16 Unicode; the US banks still contain some untranslated Japanese entries", V),
                     ("species, move and item names are in the banks inside common_rel", V)]}),
    ("fonts*", {
        "how": [("extract.py stage `fonts` (colo/font.py, layout from fn_800FD348 / fn_800FD69C)", V)],
        "ingame": [("the fonts used for all text", V)],
        "insights": [("glyph codes are Unicode, so translations are text-only where the fonts cover the language", V),
                     ("jis_1 = the 2,920 JIS level-1 kanji", V)]}),
    ("data_tables", {
        "how": [("extract.py stage `tables`: common_rel tables (colo/tables.py) and main.dol tables", V)],
        "ingame": [("the game's data: species, moves, items, trainers, battles, rooms, flags, sounds", V)],
        "insights": [("common_rel's prolog writes each table's pointer and count into main.dol globals", V)]}),
    ("audio/songs", {
        "how": [("extract.py stage `songs`: bgm_archive.fsys type-4 entries -> MIDI + instrument map (colo/song.py, colo/musyx.py); originals in original/", V)],
        "ingame": [("the music (MusyX songs)", V)],
        "insights": [("which battle plays which song: docs/engine/music.md (battle record, then arena, then trainer class)", V)]}),
    ("audio/musyx_data", {
        "how": [("extract.py stage `other`: snd_* entries of common.fsys, kept raw; stage `audio`: <bank>_bank.txt = each bank's groups, programs (-> samples), MIDI setups, layers, keymaps and every macro step with its opcode name (colo/musyx.py)", V)],
        "ingame": [("MusyX project and pool data (instrument and effect definitions)", V)], "insights": []}),
    ("audio/*", {
        "how": [("extract.py stage `audio`: the bank's sample directory + .samp file, DSP-ADPCM decoded (colo/musyx.py)", V)],
        "ingame": [("the samples of one sound bank: music instruments, sound effects or Pokémon cries", V)],
        "insights": [("nakigoe = cries; se = sound effects; music_atmos = ambience", V)]}),
    ("audio", {"how": [("extract.py stage `audio`", V)], "ingame": [("all sound samples and songs", V)], "insights": []}),
    ("other/disc_root", {
        "how": [("extract.py stage `other`: the disc files that are not .fsys archives", V)],
        "ingame": [("GBA link programs (guess), the archive table of contents (GSfsys.toc), the disc banner", G)], "insights": []}),
    ("other/*", {
        "how": [("extract.py stage `other`: entries no converter handles yet, kept raw", V)],
        "ingame": [("see the archive name and docs/engine/fsys-catalog.md", G)], "insights": []}),
    ("other", {"how": [("extract.py stage `other`", V)], "ingame": [("everything not converted yet", V)], "insights": []}),
    ("scripts/*", {
        "how": [("extract.py stage `scripts`: type-7 entries disassembled by colo/script.py (opcodes from the cms_opNN handlers, natives from lbl_802E1CF0)", V)],
        "ingame": [("the event scripts of this area's rooms: cutscenes, NPC talk, music, warps, battles", V)],
        "insights": [("checked on the opening at Outskirt Stand (grunts' scene, Willie's battle): the listing matches the scene", V),
                     ("std078_flagSetFalse really sets the flag and std079_flagSetTrue clears it (names swapped)", V)]}),
    ("scripts", {"how": [("extract.py stage `scripts`", V)], "ingame": [("every event script", V)], "insights": []}),
    ("raw_entries", {"how": [("tools/local/fsys_extract.py (single entries, by hand)", V)], "ingame": [("research scratch", V)], "insights": []}),
]


# What each non-room archive is for, per archive name (docs/engine/fsys-catalog.md, "Shared archives" and
# "Menus and screens"). Used for ui/<archive>, other/<archive> and fonts/rooms/<archive> READMEs.
ARCHIVE_PURPOSE = {
    "common": [("shared resources loaded at boot: common_rel (all data tables), MusyX project / pool / sample directories, shared models and textures", V)],
    "field_common": [("shared overworld resources: player and Rui overworld models, the '!' surprise icon (bikkuri), sparkle (kirari), treasure box (t_box_00)", V)],
    "fight_common": [("shared battle resources: battle script fight_common_scd, battle HUD textures and fonts", G)],
    "title": [("the title screen: menu script, logo demo, t_vs_* textures (the title screen's character pictures: legendaries, final starters, Wes / Rui / Espeon / Umbreon, the GBA protagonists; which set shows is not traced)", G)],
    "topmenu": [("the main menu after the title screen (new game / continue / options)", G)],
    "name_entry_menu": [("the name entry screen", G)], "pcbox_name_entry_menu": [("name entry for boxes / Pokémon in the PC", G)],
    "pocket_menu": [("the Bag (pocket = bag)", G)], "pcbox_pocket_menu": [("the Bag as opened from the PC (item storage)", G)],
    "pcbox_menu": [("the PC box screens (model `savor`)", G)], "pcbox_poke_box": [("the PC box Pokémon storage screens", G)],
    "pda_menu": [("the P★DA, the player's in-game device menu (mail, memo, Pokémon data)", G)],
    "waza_menu": [("the move (waza) menu: move details and reordering", G)],
    "world_map": [("the world map and the bike travel scenes (bike_* models, 15 cam_move_demo_* cameras)", V)],
    "evolution": [("the evolution scene's map", G)],
    "colosseumbattle_menu": [("menus for Colosseum-mode battles (battle mode)", G)],
    "colosseumbattleconnection_menu": [("menus for link (GameCube-to-GameCube) battles", G)],
    "toolbattle_menu": [("menus for quick battles (guess for 'tool battle')", G)],
    "pokemonchange_menu": [("trading (poke_exchange_* = Pokémon exchange)", G)],
    "pokecoupon_menu": [("the Poké Coupon exchange", G)],
    "ex_coupon": [("Bonus Disc content: Poké Coupon exchange extras (all ex_ archives are Bonus Disc content)", V)],
    "ex_shrine": [("Bonus Disc content: the Celebi event at the Agate shrine (cerebi_dat, relive_gpt)", V)],
    "ex_title": [("Bonus Disc content: title extras (kakutyou = expansion)", V)], "ex_top": [("Bonus Disc content: top-menu extras", V)],
    "carde_menu": [("e-Reader battle cards (Card e): card01_bin..card21_bin; a Japan-only feature in the released game", G)],
    "DNA_key": [("map `dnakey` with 2 cameras: purpose unknown", G)],
    "debug_menu": [("the developers' debug menu (REL module debugMenuData)", V)],
    "waza_viewer": [("the developers' move-animation viewer (waza_viewer_rel, uses the M1_water_colo map)", V)],
    "genius_logo": [("boot logo: Genius Sonority", V)], "nintendo_logo": [("boot logo: Nintendo", V)],
    "pokemon_logo": [("placeholder entry `dummy`: the screen plays a .thp movie instead", G)],
    "opening_demo": [("placeholder entry `dummy`: the opening plays a .thp movie instead", G)],
    "auto_demo": [("placeholder entry `dummy`: the attract demo plays a .thp movie instead", G)],
    "staffroll": [("placeholder entry `dummy`: the credits play a .thp movie instead", G)],
    "keydisc": [("a 32-byte `key` entry (Bonus Disc / key disc check, guess)", G)],
    "mail000": [("mail stationery", G)], "mail001": [("mail stationery", G)], "mail002": [("mail stationery", G)],
    "camera_shake_data": [("camera shake presets: short / medium / long x weak / medium / strong", V)],
}


def notes_for(relpath):
    rel = relpath.replace("\\", "/").strip("/")
    for pat, n in NOTES:
        if fnmatch.fnmatchcase(rel, pat): return n
    return None
