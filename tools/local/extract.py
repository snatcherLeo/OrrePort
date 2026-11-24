"""Read-only extractor: unpack the player's own disc image into viewable files under extracted/.
Never commit or push anything from extracted/ (the project rules). Documented in docs/engine/extractor.md.

Usage (repo root):  python -I tools/local/extract.py <image.iso> <what> [--out <folder>]
  The default output is the main checkout's extracted/ from any worktree ($ORRE_EXTRACTED or --out override it).
  what = pokemon | trainers | npcs | maps | ui | effects | text | fonts | scripts | tables | audio | songs | movies | other | all
Output layout: extracted/README.txt (written by this tool) and docs/tools.md "Extracted folder layout".

Formats: docs/engine/disc-files.md (FSYS, LZSS), textures.md, messages.md, game-data-tables.md,
models-animation.md. Everything is decoded from the disc image given on the command line."""
import json, os, re, struct, sys, zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # our own reader package (tools/local/colo)
from colo.disc import *
from colo.fsys import *
from colo.gx import *
from colo.msg import *
from colo.tables import *
from colo.hsd import *
from colo.musyx import *
from colo.pkx import *
from colo.font import *
from colo.folder_notes import notes_for
from colo.script import listing, native_table, OPS
from colo import world
from colo import collision
from colo import hsdcam
from colo.folder_notes import ARCHIVE_PURPOSE

sys.stdout.reconfigure(encoding="utf-8")

# ---------------------------------------------------------------- disc and FSYS




def safe(name): return re.sub(r"[^A-Za-z0-9_.-]", "_", name)

def mkdir(p): os.makedirs(p, exist_ok=True); return p

# ---------------------------------------------------------------- PNG and GX textures
def png(path, w, h, rgba):
    raw = b"".join(b"\0" + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))






# ---------------------------------------------------------------- text





# ---------------------------------------------------------------- common_rel tables




# ---------------------------------------------------------------- output layout, READMEs, manifest
TOOL = "tools/local/extract.py (read-only extractor; docs/tools.md, docs/engine/extractor.md)"
AREAS = {"S1": "Outskirt_Stand", "S2": "Team_Snagem_Hideout", "M1": "Phenac_City", "M2": "Pyrite_Town",
         "M3": "Agate_Village", "M4": "The_Under", "D1": "Shadow_Pokemon_Lab", "D2": "Mt_Battle",
         "D4": "Realgam_Tower", "T1": "Colosseum_Arena_T1"}  # verified by snatcherLeo except T1 (docs/engine/fsys-catalog.md)
TYPE_EXT = {0: "bin", 1: "map.dat", 2: "dat", 3: "collision.bin", 4: "bgm.bin", 5: "msg", 6: "font.bin",
            7: "script.bin", 9: "png", 10: "particles.bin", 12: "camera.bin", 14: "rel", 15: "pkx", 16: "wzx",
            17: "gfl.bin", 18: "particles.bin", 19: "rel"}
TYPE_FMT = {0: "raw data", 1: "HSD map model (.dat)", 2: "HSD model (.dat)", 3: "collision (raw)", 4: "BGM sequence (raw)",
            5: "message bank", 6: "font (raw)", 7: "script (raw)", 9: "PNG", 10: "particles (raw)", 12: "camera (raw)",
            14: "REL module", 15: "PKX (header + HSD model + animation slots)", 16: "WZX move effect (raw)",
            17: "GFL lens flare", 18: "particles (raw)", 19: "REL module"}

MANIFEST = []   # rows: path, archive, entry, original name, format, size, gx format, linked file
ORIG_EXT = {0: "bin", 1: "dat", 2: "dat", 3: "col", 4: "song", 5: "msg", 6: "fnt", 7: "scd", 9: "gtx", 10: "gpt1",
            12: "cam", 14: "rel", 15: "pkx", 16: "wzx", 17: "gfl", 18: "gpt1", 19: "rel"}
ORIG_FMT = {"thp": "THP video (Motion-JPEG + ADPCM audio)", 0: "generic data", 1: "HSD map model (DAT)", 2: "HSD model (DAT)", 3: "collision data",
            4: "MusyX song (arrangement)", 5: "message bank (UTF-16 text)", 6: "font (glyph table + 4-bit bitmaps)",
            7: "script bytecode", 9: "GS texture (GStextureHandle header + GameCube tiled pixels)",
            10: "GPT1 particle bank", 12: "camera animation", 14: "REL module", 15: "PKX battle model",
            16: "WZX move-effect sequence", 17: "GFL lens flare (flareInit)", 18: "GPT1 particle bank (not linked)", 19: "REL module (code)"}
ORIGINALS = {}  # original/ folder -> list of [file, format, converted files, archive, entry, info]
DISC = None     # set in main(); used to look up archive and entry ids
_ARCH_IDS = {}
V, G = "verified", "guess"
# Per resource type: (code that loads / uses it, port notes). Each line: (text, status).
TYPE_DOC = {
    9: ([("type-9 callbacks floorReadTexPreFunc / floorReadTexPostFunc (resource table lbl_8036C2A0)", V),
         ("GStextureHandle header; GS -> GX format map GStextureGetGXformat (0x800EF3E0, gs_texture_getters_exact_800EF3E0.c)", V)],
        [("Read the 0x80-byte GStextureHandle header (u16 width, u16 height, u8 bpp, u8 mip count, s32 GS format, u32 palette format); pixel data at the header's +0x28 offset (0x80 on disc).", V),
         ("Decode GameCube tiled pixels (tools/local/colo/gx.py is the reference); a GPU port can upload CMPR as BC1 directly.", V),
         ("Portrait sheets are 42x42 cells: top = normal, bottom = shiny, right column = facing the other way (fn_8010B9E8).", V)]),
    15: ([("load: fn_801DD5E8 / sequenceLoad (src/game/sequence.c); model choice fn_801DE190 (Pokémon table lbl_80370BD0, shiny-model table lbl_80374F20, Unown forms lbl_80374E40)", V),
          ("animation: wazaSequencePokemonMotionStart, wazaSequenceSysGetWazaTime (wazaSequenceEntry.c, wazaSequenceSys_tail.c)", V)],
         [("Header 0x40: model size, GPT1 size, slot count (16/17), +0x0C value, 5, flags, three sub-animation refs; then the HSD model (32-byte aligned), the optional GPT1 bank, 17 x 0xD0 animation slots, and a 20-byte shiny colour block (4 u32 channel routes + 4 bytes modulation stored A,B,G,R).", V),
          ("Time points are 60 Hz frames, rescaled by the video rate at load (keep logic at 60 Hz).", V),
          ("Render the HSD model with the decomp's HSD code; apply the shiny recolour only when the species has no separate shiny model.", V)]),
    1: ([("type-1 callbacks floorReadMap* (resource table lbl_8036C2A0)", V)],
        [("HSD archive (DAT) holding the room's map scene; load with the decomp's HSD archive code.", V)]),
    2: ([("type-2 callbacks floorReadObj* (resource table lbl_8036C2A0); NPC models through people* code", V)],
        [("HSD archive (DAT); load with the decomp's HSD code. Joint-level particle references call gsParticlePtclHook.", V)]),
    5: ([("type-5 callbacks floorReadMsg*; lookup GSmsgGetGSchar (0x800FA280, gs_msg.c)", V)],
        [("UTF-16 big-endian text; 0xFFFF + one byte = control code; entries {u32 id, u32 offset} sorted by id (binary search).", V)]),
    6: ([("type-6 callbacks floorReadFont*; GSmsgFontOpen (gs_msg.c); glyph draw fn_800FD348 / fn_800FD69C", V)],
        [("Glyph records {u16 char, u8 width, u8 height, s8 y offset, u24 offset}; bitmaps 4-bit, rows of ceil(width/2) bytes.", V),
         ("Quirk: bitmap data starts at font header + 8 + data offset (not header + data offset).", V),
         ("Room fonts carry only the extra glyphs that room's text needs; the main fonts are in common_rel.", V)]),
    4: ([("type-4 callbacks floorReadBGM*; playback soundPlay (gs_range_801653CC_exact_801654E0.c) and the MusyX sequencer (src/musyx/runtime/seq.c, ARR layout)", V)],
        [("MusyX arrangement: offsets to tracks, tempo map, track sections; tempo in the info word; 16 loop points. Plays the instrument samples of the music bank.", V),
         ("For music replacement, hook soundPlay / the battle-music lookup instead of decoding the song (docs/engine/music.md).", V)]),
    3: ([("type-3 callbacks floorReadCol* (resource table lbl_8036C2A0); floorReadColPostFunc -> fn_8010CFE4 fixes the offsets (_offsetCCD__FP12CCD_FILEHEAD) and makes it the current collision; debug drawer GScolsys2Draw (src/game/gs_range_8010CBD0.c)", V)],
        [("Header {u32 records offset, u32 count}; 0x40-byte records: translate / rotate / scale (9 f32), six group pointers (+0x24..+0x38), u16 flags (+0x3C, bit 0 = skip); group {triangles, count, extra, extra}; triangles 0x34 bytes (3 vertices, normal, 2 attribute bytes) or 0x30 (group +0x38). Reader: tools/local/colo/collision.py; docs/engine/collision.md.", V)]),
    7: ([("type-7 callbacks floorReadScript*; run by the cms_ script VM (gs_thread.c)", V)], [("Script bytecode (docs/engine/script-vm.md); a disassembler is the next step.", V)]),
    10: ([("type-10 callbacks floorReadParticle*; gsParticleBankLoad (field_range_80117E58.c)", V)], [("GPT1 particle bank (HSD particle system): decoded by colo/gpt1.py into particles/<name>.txt + textures (docs/engine/particles.md).", V)]),
    18: ([("type-18 callbacks floorReadNotLinkedParticle*", V)], [("GPT1 particle bank not linked to a model (the sleep and freeze loops): decoded like type 10 (docs/engine/particles.md).", V)]),
    12: ([("type-12 callbacks floorReadCamera*: floorReadCameraPostFunc (0x8011487C) parses the HSD archive, takes scene_data +0x04 and builds the camera (fn_800D27FC)", V)], [("HSD archive; scene_data -> camera {CObjDesc (eye / interest WObjDescs, roll, near, far, fov, aspect), CameraAnim list (camera / eye / interest AObjDescs with FObj keyframe tracks)}; reader tools/local/colo/hsdcam.py; keyframes decoded by colo/fobj.py (frame, interpolation op, value, slope per key).", V)]),
    14: ([("type-14 callbacks fn_8017F3F8 / fn_8017F484 (gs_range_8017F2C4.c): copy, OSLink (fn_8009ED4C), run the prolog", V)],
         [("Dolphin REL module; the prolog fills table pointers in the executable (docs/engine/game-data-tables.md). A port can link it as the game does, or read the tables directly.", V)]),
    19: ([("type-19 callbacks floorReadRelModule*", V)], [("Dolphin REL code module (debug menu).", V)]),
    16: ([("type-16 callback floorReadWZXPreFunc; wazaSequenceLoadData (wazaSequence.c)", V)],
         [("WZX sequence: 0x70-byte entries pinned to animation time points of the attacker's slot (docs/engine/models-animation.md).", V)]),
    17: ([("type-17 callbacks floorReadGFL*", V)], [("Unknown format (one entry on the disc).", G)]),
    0: ([("type-0 callback floorReadNormalPreFunc (generic data), or read directly by its user", V)], []),
}
SDIR_DOC = ([("MusyX dataInsertSDir / sample lookup (src/musyx/runtime/synthdata.c, SdirDataT)", V)],
            [("0x20-byte entries {u16 id, u16 refs, u32 offset into .samp, u32 addr, u32 base note << 24 | rate, u32 sample count (top byte = format, 0 = DSP-ADPCM), u32 loop offset, u32 loop length, u32 ADPCM info offset}; ADPCM info: 8 coefficient pairs.", V),
             ("Decode DSP-ADPCM in software (tools/local/colo/musyx.py); 8-byte frames of 14 samples.", V)])

def arch_id(name):
    if not _ARCH_IDS and DISC is not None:
        for path in DISC.files:
            if path.endswith(".fsys"):
                h = DISC.read(DISC.files[path][0], 12)
                if h[:4] == b"FSYS": _ARCH_IDS[path[:-5]] = struct.unpack(">I", h[8:12])[0]
    return _ARCH_IDS.get(name)

def entry_id(archive, entry):
    """(entry id, LZSS-compressed?) of entry 'k (name)' of an archive, read from its FSYS entry."""
    m = re.match(r"(\d+) \(", str(entry))
    if not m or DISC is None or f"{archive}.fsys" not in DISC.files: return None
    arc = DISC.file(f"{archive}.fsys"); k = int(m.group(1))
    blk = struct.unpack(">I", arc[0x18:0x1C])[0]; tab = struct.unpack(">I", arc[blk:blk + 4])[0]
    eo = struct.unpack(">I", arc[tab + 4 * k:tab + 4 * k + 4])[0]
    return struct.unpack(">I", arc[eo:eo + 4])[0], bool(struct.unpack(">I", arc[eo + 0x0C:eo + 0x10])[0] & 0x80000000)
    return None

def tex_details(d):
    try:
        w, h, bpp, mips = struct.unpack(">HHBB", d[:6]); fmt = struct.unpack(">i", d[8:12])[0]; tl = struct.unpack(">I", d[12:16])[0]
    except struct.error:
        return []
    gx = GXF.get(fmt, ("?",))[0]
    pal = {0: "IA8", 1: "RGB565", 2: "RGB5A3"}.get(tl, str(tl)) if gx in ("C4", "C8", "C14X2") else "none"
    return [(f"{w} x {h} pixels, GS format {fmt:#x} = GX {gx}, {bpp} bits per pixel, {mips} mip level(s), palette {pal}, {len(d)} bytes", V)]
LINKS = {}      # path -> linked path (converted <-> original)
SOURCES = {}    # folder -> {archive: set(entries)}
READMES = {}    # folder -> dict(what, fmt, evidence, docs, checks)
DETAILS = {}    # folder -> [(text, status)]: facts about this exact folder from the game's tables (rooms, battles, moves)

def detail(folder, items):
    DETAILS.setdefault(os.path.abspath(folder), []).extend(i for i in items if i)
REPO = os.getcwd()

# Top of every generated README.md and CATALOG.md (snatcherLeo, 2026-10-16): docs/ is the source of truth, not these pages.
BANNER = ["> **How to use this:** Browse here, learn from docs/. Files in extracted/ are regenerated by extract.py; their "
          "READMEs are convenience summaries, not the source of truth. The official documentation is in docs/ (start with "
          "docs/engine/index.md, docs/tools.md and docs/engine/extractor.md). If they conflict, docs/ wins. Never hand-edit "
          "READMEs here; put lasting notes in docs/.", ""]


def note(path, archive, entry, original, fmt, size="", gx=""):
    MANIFEST.append([os.path.abspath(path), archive, entry, original, fmt, size, gx, ""])
    if archive:
        SOURCES.setdefault(os.path.dirname(os.path.abspath(path)), {}).setdefault(archive, set()).add(str(entry))

def readme(folder, what, fmt, source="", evidence=None, docs=None, checks=None):
    """Register a folder description; README.md files are written at the end (finish_readmes).
    evidence: list of (claim, how we know, 'verified (code)' / 'verified (snatcherLeo)' / 'guess')."""
    mkdir(folder)
    READMES[os.path.abspath(folder)] = dict(what=what, fmt=fmt, source=source, evidence=evidence or [],
                                            docs=docs or [], checks=checks or [])

def finish_readmes():
    walked = set()
    root = getattr(finish_readmes, "out", None)
    if root:
        for dp, dns, fns in os.walk(root):
            dns[:] = [d for d in dns if d != "original"]
            walked.add(os.path.abspath(dp))
    for folder in sorted(set(READMES) | set(SOURCES) | walked):
        info = READMES.get(folder)
        if info is None:  # a sub-folder without its own description: describe it from its parent
            parent = os.path.dirname(folder)
            while parent not in READMES and len(parent) > len(REPO): parent = os.path.dirname(parent)
            if parent not in READMES and folder not in SOURCES and folder not in DETAILS:
                continue  # not produced by this run (single-stage runs must not rewrite other stages' READMEs)
            pi = READMES.get(parent, dict(what="Extracted files.", fmt="see manifest.csv", source="", evidence=[], docs=[], checks=[]))
            info = dict(pi, what=f"Part of `{os.path.basename(parent)}`: {pi['what']}")
        old = os.path.join(folder, "README.txt")
        if os.path.exists(old): os.remove(old)
        def link(doc):
            rel = os.path.relpath(os.path.join(REPO, doc), folder).replace(os.sep, "/")
            return f"[{doc}]({rel})"
        lines = [f"# {os.path.basename(folder)}", ""] + BANNER + ["## What this is", "", info["what"], ""]
        OUTROOT = getattr(finish_readmes, "out", None)
        fn = notes_for(os.path.relpath(folder, OUTROOT)) if OUTROOT else None
        if fn:
            def bl(items): return [f"- {t} *({st})*" for t, st in items] or ["- (nothing recorded yet)"]
            lines += ["## How it was extracted", ""] + bl(fn["how"]) + [""]
            lines += ["## In-game", ""] + bl(fn["ingame"]) + [""]
            if fn["insights"]: lines += ["## Insights", ""] + bl(fn["insights"]) + [""]
        if DETAILS.get(folder):
            lines += ["## In the game data", ""] + [f"- {t} *({st})*" for t, st in DETAILS[folder]] + [""]
        lines += ["## How we know", ""]
        if info["evidence"]:
            lines += ["| Claim | Evidence | Status |", "|---|---|---|"]
            lines += [f"| {c} | {e} | {st} |" for c, e, st in info["evidence"]]
        else:
            lines += ["Each line above is marked *verified* (from code or data) or *guess*; anything not marked comes from "
                      "the archive and entry names only."]
        lines += ["", "## Sources", ""]
        srcs = SOURCES.get(folder, {})
        if info["source"]: lines += [info["source"], ""]
        if srcs:
            lines += ["| Archive | Entries |", "|---|---|"]
            for arch in sorted(srcs):
                ents = sorted(srcs[arch])
                lines.append(f"| `{arch if ('/' in arch or '.' in arch) else arch + '.fsys'}` | " + ", ".join(f"`{e}`" for e in ents[:40]) + (f" ... ({len(ents)} entries)" if len(ents) > 40 else "") + " |")
        elif not info["source"]:
            lines += ["See the sub-folders' README.md files and `extracted/manifest.csv`."]
        lines += ["", "## Format and tool", "", info["fmt"], "",
                  f"Produced by `{TOOL}`. Every file is listed in `extracted/manifest.csv` (path, source archive, entry, "
                  "original name, format, size, GameCube texture format).", ""]
        if os.path.isdir(os.path.join(folder, "original")):
            lines += ["The untouched disc entries behind these files are in [`original/`](original/README.md).", ""]
        lines += ["## Related docs", ""] + ([f"- {link(d)}" for d in info["docs"]] or ["- [docs/tools.md](" + os.path.relpath(os.path.join(REPO, "docs/tools.md"), folder).replace(os.sep, "/") + ")"]) + [""]
        if info["checks"]:
            lines += ["## Open visual checks", ""] + [f"- {link('the project notes')} item {c}" for c in info["checks"]] + [""]
        lines += ["---", "Extracted from the player's own disc. Never commit or push this folder (it is git-ignored)."]
        open(os.path.join(folder, "README.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")

def keep_original(folder, archive, entry, name, ftype, data, converted, fmt_extra="", ext=None, info=None):
    """Write the untouched entry (unpacked from the archive's LZSS) to <folder>/original/<name>.<ext>,
    record it for the original/ README and link it to its converted file(s) in the manifest."""
    odir = mkdir(os.path.join(folder, "original"))
    path = os.path.join(odir, f"{safe(name)}.{ext or ORIG_EXT.get(ftype, f't{ftype}')}")
    if not os.path.exists(path) or os.path.getsize(path) != len(data): open(path, "wb").write(data)
    conv = [os.path.abspath(c) for c in (converted if isinstance(converted, (list, tuple)) else [converted]) if c]
    fmt = ORIG_FMT.get(ftype, f"type {ftype}") + (f", {fmt_extra}" if fmt_extra else "")
    rows = ORIGINALS.setdefault(os.path.abspath(odir), [])
    if not any(r[0] == os.path.abspath(path) for r in rows):
        info = dict(info or {})
        info.setdefault("type", ftype)
        if ftype == 9 and ext is None: info.setdefault("format", tex_details(data))
        info.setdefault("size", len(data))
        rows.append([os.path.abspath(path), fmt, conv, archive, entry, info])
        MANIFEST.append([os.path.abspath(path), archive, entry, name, "original: " + fmt, str(len(data)), "", ";".join(conv)])
    else:
        for r in rows:
            if r[0] == os.path.abspath(path): r[2] += [c for c in conv if c not in r[2]]
    for c in conv: LINKS[c] = os.path.abspath(path)
    return path

def finish_originals():
    def item(t, st): return f"- {t} *({st})*"
    for odir, rows in ORIGINALS.items():
        rel = lambda p: os.path.relpath(p, odir).replace(os.sep, "/")
        lines = [f"# original/ ({os.path.basename(os.path.dirname(odir))})", ""] + BANNER + [
                 "The exact entries from the disc behind the converted files one folder up: unpacked from the archive's",
                 "LZSS compression, otherwise byte-for-byte as stored, under their original entry names with a format",
                 "extension. Why we keep them: `docs/tools.md` (\"Why the originals are kept\"). Each detail below is",
                 "marked *verified* (from code or data) or *guess*.", "",
                 "| Original file | In-game format | Converted file(s) | Source archive | Entry |", "|---|---|---|---|---|"]
        for path, fmt, conv, arch, entry, info in sorted(rows, key=lambda r: r[0]):
            cv = ", ".join(f"[{os.path.basename(c)}]({rel(c)})" for c in conv[:6]) + (f" (+{len(conv) - 6})" if len(conv) > 6 else "") or "(none yet: kept raw)"
            src = arch if ("/" in arch or arch.endswith((".dol", ".samp"))) else f"{arch}.fsys"
            lines.append(f"| `{os.path.basename(path)}` | {fmt} | {cv} | `{src}` | {entry} |")
        for path, fmt, conv, arch, entry, info in sorted(rows, key=lambda r: r[0]):
            t = info.get("type"); code, port = (SDIR_DOC if info.get("sdir") else TYPE_DOC.get(t, ([], [])))
            lines += ["", f"## `{os.path.basename(path)}`", ""]
            names = info.get("names", [])
            if names: lines += ["**Names**", ""] + [item(n, st) for n, st in names] + [""]
            ids = list(info.get("ids", []))
            aid = arch_id(arch); eid = entry_id(arch, entry)
            if aid is not None: ids.append((f"archive `{arch}.fsys`: archive id {aid:#06x} (FSYS header +0x08; GSfsys.toc maps it to the name)", V))
            packed = None
            if eid is not None: eid, packed = eid
            if eid is not None: ids.append((f"entry id {eid:#010x} (entry +0x00: u16 id {eid >> 16:#06x}, type byte {(eid >> 8) & 0xFF:#04x} = resource type {((eid >> 8) & 0xFF) // 2})", V))
            if ids: lines += ["**Identifiers and references**", ""] + [item(n, st) for n, st in ids] + [""]
            fdet = list(info.get("format", []))
            if eid is not None:
                fdet.append((("stored LZSS-compressed in the archive (FSYS entry flag 0x80000000), unpacked here" if packed
                              else "stored uncompressed in the archive") + f"; {info.get('size', 0)} bytes", V))
            fdet = [(fmt, V)] + fdet
            lines += ["**Format**", ""] + [item(n, st) for n, st in fdet] + [""]
            allcode = list(code) + list(info.get("code", []))
            if allcode: lines += ["**Code that loads or uses it**", ""] + [item(n, st) for n, st in allcode] + [""]
            allport = list(port) + list(info.get("port", []))
            if allport: lines += ["**Port notes**", ""] + [item(n, st) for n, st in allport] + [""]
        lines += ["---", "Extracted from the player's own disc. Never commit or push this folder (it is git-ignored)."]
        open(os.path.join(odir, "README.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")

GUIDE = """## How to find things

| You want | Look in |
|---|---|
| A Pokémon's model, portraits, shiny model | `pokemon/<NNN>_<Name>/` (NNN = internal species index; the README gives the National Dex number) |
| A trainer's battle model, who uses it, in which battles | `trainers/<file>_<Class>/` ("In the game data") |
| An overworld character (NPC, player, Rui) | `npcs/<name>/` |
| A room: map, objects, collision, room id, battles, music, items on the ground | `maps/<prefix>_<Area>/<room>/` (README "In the game data"); `_bf` = its battle-field version |
| What happens in a room (cutscenes, talk, warps, music) | `scripts/<prefix>_<Area>/<room>__<script>.txt` |
| A move's animation files and its English name | `effects/moves/<romanized Japanese move name>/` (README lists move id and name) |
| Any game text | `text/` (one file per message bank; search for the words) |
| Fonts and which characters exist | `fonts/` (`COVERAGE.md`) |
| Species, moves, items, trainers, battles, rooms as tables | `data_tables/` (`species.json`, `moves_decoded.csv`, `items_decoded.csv`, raw CSVs) |
| Sound effects, cries, instrument samples | `audio/<bank>/` (WAV); songs (sequences) in `audio/songs/` |
| Menus and screens | `ui/<archive>/` (textures), `other/<archive>/` (raw data) |
| The exact bytes from the disc | any `original/` folder (with a README per file: names, ids, format, code that loads it) |

## What is decoded and what is raw

Decoded (readable files): textures and portraits (PNG), models (`.dat` for the Blender GameCube
importer, PKX split), text (UTF-8), fonts (PNG sheets + character maps), scripts (listings), data
tables (CSV / JSON, partly decoded), sound samples (WAV), collision (OBJ). Raw (kept in their original
format, with READMEs): cameras, particles (GPT1), move-effect sequences (WZX), songs, room modules
(REL), MusyX project data. The full list with status is `docs/tools.md`, "Extraction coverage".

## How the READMEs are organised

Every folder README has: **What this is**, **How it was extracted**, **In-game** (what the game uses
it for), **Insights** (how the engine is built around it), **In the game data** (facts from the
game's own tables for that exact folder: room ids, battles, music, items, trainers, move names),
**How we know**, **Sources** (archive and entry), **Format and tool**, **Related docs** and open
**visual checks**. Every statement is marked *verified* (from code or data) or *guess*. All of it is
generated from committed sources (`tools/local/extract.py`, `tools/local/colo/folder_notes.py`,
`tools/local/colo/world.py`), so a fresh extraction recreates it.
"""

def write_top_readme(out):
    """extracted/README.md: a guide to the folder, then every directory (two levels) with its description and README link."""
    lines = ["# extracted/", ""] + BANNER + ["Everything unpacked from the player's own disc by `tools/local/extract.py` (git-ignored; never",
             "commit or push it). Each folder has a README.md; every file is listed in `manifest.csv` (path, source",
             "archive, entry, original name, format, size, GameCube texture format, linked original/converted file).",
             "Every component keeps its untouched disc entries in an `original/` subfolder next to the converted files.", "",
             "Regenerate everything (about 5 minutes): `python -I tools/local/extract.py \"<your disc image>\" all`", "",
             "```", LAYOUT.strip(), "```", "", GUIDE, "## Directories", ""]
    def desc(folder):
        info = READMES.get(os.path.abspath(folder))
        what = info["what"] if info else ""
        if not what:  # folder registered in an earlier run: read its README's "What this is"
            try:
                t = open(os.path.join(folder, "README.md"), encoding="utf-8").read()
                what = t.split("## What this is", 1)[1].split("##", 1)[0].strip()
            except (OSError, IndexError):
                what = ""
        what = what.split(". ")[0].rstrip(".")
        return (what + ".") if what else ""
    def count(folder):
        n = sum(len(fs) for _, _, fs in os.walk(folder))
        return f" ({n:,} files)"
    for top in sorted(d for d in os.listdir(out) if os.path.isdir(os.path.join(out, d))):
        tp = os.path.join(out, top)
        lines.append(f"- [`{top}/`]({top}/README.md){count(tp)}: {desc(tp)}")
        subs = sorted(d for d in os.listdir(tp) if os.path.isdir(os.path.join(tp, d)) and d != "original")
        if len(subs) <= 40:
            for sd in subs: lines.append(f"  - [`{sd}/`]({top}/{sd}/README.md) {desc(os.path.join(tp, sd))}".rstrip())
        else:
            lines.append(f"  - {len(subs)} sub-folders (e.g. " + ", ".join(f"[`{sd}/`]({top}/{sd}/README.md)" for sd in subs[:5]) + ")")
    lines += ["", "---", "Extracted from the player's own disc. Never commit or push this folder (it is git-ignored)."]
    open(os.path.join(out, "README.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")

def write_manifest(out):
    with open(os.path.join(out, "manifest.csv"), "w", encoding="utf-8", newline="") as f:
        f.write("path,source_archive,entry,original_name,format,size,gamecube_texture_format,linked_file\n")
        for row in MANIFEST: f.write(",".join('"' + str(c).replace('"', "'") + '"' for c in row) + "\n")

def title(name): return re.sub(r"[^A-Za-z0-9_.-]", "_", name.title().replace(" ", "_")).strip("_") or "Unnamed"

def is_trainer_pkx(name): return bool(re.search(r"_[a-d]\d+$|_[mf]_", name))

# ---------------------------------------------------------------- HSD model textures

OBJ_FAILED = []
def save_obj(mdir, readable, dat, archive, entry, original):
    """The model as OBJ + MTL (bind pose) with its textures as PNG (colo/hsdmodel.py, docs/engine/hsd-models.md)."""
    from colo import hsdmodel
    try:
        m = hsdmodel.parse_model(dat)
    except Exception as ex:  # a model the walk can't read stays .dat only (listed at the end of the run)
        OBJ_FAILED.append(f"{archive}:{original}: {ex!r}"[:200]); return []
    if not m["meshes"]: return []
    files = []; names = {}
    for k, (off, (w, h, rgba, fmt)) in enumerate(sorted(m["textures"].items())):
        nm = f"{readable}_obj_tex{k:02d}.png"; p = os.path.join(mdir, nm); png(p, w, h, rgba); names[off] = nm
        note(p, archive, entry, original, "PNG (texture used by the OBJ)", f"{w}x{h}"); files.append(p)
    obj, mtl = hsdmodel.to_obj(m, readable, names)
    out = [("obj", obj, "OBJ mesh (bind pose; colo/hsdmodel.py)"), ("mtl", mtl, "OBJ material library")]
    if any(me["hidden"] for me in m["meshes"]):
        hobj, hmtl = hsdmodel.to_obj(m, readable, names, hidden=True, mtl_name=f"{readable}_hidden")
        out += [("_hidden.obj", hobj, "OBJ mesh: the meshes on hidden joints (JOBJ_HIDDEN; helper volumes or parts animations show)"),
                ("_hidden.mtl", hmtl, "OBJ material library")]
    for ext, text, fmt in out:
        p = os.path.join(mdir, f"{readable}{ext if ext.startswith('_') else '.' + ext}"); open(p, "w", encoding="utf-8").write(text)
        note(p, archive, entry, original, fmt); files.append(p)
    return files

def save_model(folder, readable, data, archive, entry, original, is_pkx, ftype=2, info=None):
    """Model in its original format with a readable name, plus model/ textures as PNG."""
    mdir = mkdir(os.path.join(folder, "model"))
    if is_pkx:
        p = os.path.join(mdir, f"{readable}.pkx"); open(p, "wb").write(data)
        note(p, archive, entry, original, TYPE_FMT[15] + " (same bytes as original/, readable name)")
        conv_files = [p]
        parts = parse_pkx(data); dat = parts["model"]
        p = os.path.join(mdir, f"{readable}.dat"); open(p, "wb").write(dat)
        note(p, archive, entry, original, "HSD model (.dat) cut from the PKX, for the Blender GameCube importer"); conv_files.append(p)
        if parts["gpt1"]:  # GPT1 ambient particle bank (docs/engine/models-animation.md, "Ambient particle effects")
            p = os.path.join(mdir, f"{readable}_ambient_particles.gpt1"); open(p, "wb").write(parts["gpt1"])
            note(p, archive, entry, original, "GPT1 particle bank (ambient effect linked to the model)"); conv_files.append(p)
            conv_files += save_particles(mdir, archive, entry, original, parts["gpt1"], keep=False, label=f"{readable}_ambient_particles")
    else:
        dat = data; p = os.path.join(mdir, f"{readable}.dat"); open(p, "wb").write(dat)
        note(p, archive, entry, original, TYPE_FMT.get(ftype, TYPE_FMT[2]) + " (same bytes as original/, readable name)")
        conv_files = [p]
    anims = hsd_animations(dat)  # the animation list of each model set (not the PKX slot table) (docs/engine/hsd.md, "Model sets and their animations")
    if anims and any(anims):
        p = os.path.join(mdir, f"{readable}_animations.csv")
        with open(p, "w", encoding="utf-8") as f:
            f.write("model_set,animation,end_frame\n")
            for ms, slots in enumerate(anims):
                for slot, end in slots:
                    f.write(f"{ms},{slot},{end:g}\n")
        note(p, archive, entry, original, "CSV: the animation list of each model set (index, length = end frame); the PKX slot table (models-animation.md) points into it")
        conv_files.append(p)
    conv_files += save_obj(mdir, readable, dat, archive, entry, original)
    n = 0; tex_files = []
    for i, (w, h, gx, rgba) in enumerate(hsd_textures(dat)):
        tdir = mkdir(os.path.join(folder, "textures"))
        p = os.path.join(tdir, f"{readable}_tex{i:02d}_{w}x{h}_{gx}.png"); png(p, w, h, rgba)
        note(p, archive, entry, original, "PNG (texture from the HSD model; original pixels are inside ../model/original/)", f"{w}x{h}", gx)
        tex_files.append(p); n += 1
    if is_pkx:
        h = parts["header"]; info = dict(info or {})
        info.setdefault("format", [])
        info["format"] = info["format"] + [(f"PKX: HSD model {h['model_size']} bytes, GPT1 particle bank {h['gpt1_size']} bytes, {h['slot_count']} animation slots, +0x0C value {h['kind']}, flags {h['flags']:#x}, sub-animation refs {h['sub_anim_refs']}", V),
                                           (f"{len(tex_files)} textures found in the model (heuristic scan; PNGs in ../textures/)", G if tex_files else V)]
    keep_original(mdir, archive, entry, original, 15 if is_pkx else ftype, data, conv_files + tex_files, info=info)
    return n

def save_texture(path, d, archive, entry, original):
    r = decode_texture_file(d)
    if not r:
        keep_original(os.path.dirname(path), archive, entry, original, 9, d, [], "format not decoded yet"); return
    w, h, rgba, gx = r; png(path + ".png", w, h, rgba)
    note(path + ".png", archive, entry, original, "PNG", f"{w}x{h}", gx)
    keep_original(os.path.dirname(path), archive, entry, original, 9, d, path + ".png", f"GX {gx}, {w}x{h}")

# ---------------------------------------------------------------- commands



_WORLD = {}
def world_ctx(disc):
    """Tables the per-folder README details need (cached): rooms, arenas, battles, treasure, items, songs."""
    if _WORLD: return _WORLD
    rel = common_rel(disc); toc = gsfsys_toc(disc); msgs = all_text(disc); dol = disc.dol()
    fl = world.floors(rel, toc)
    rd = world.dol_reader(dol)
    nid = struct.unpack(">I", rd(0x80478BD8, 4))[0]; nrec = struct.unpack(">I", rd(0x80478BB0, 4))[0]
    lookup = struct.unpack(f">{nid}H", rd(0x803681E8, 2 * nid))
    items = {iid: clean(msgs.get(struct.unpack(">I", rd(0x80363CE8 + slot * 0x28 + 0x10, 4))[0], ""))
             for iid, slot in enumerate(lookup) if slot < nrec}
    moves = {i: clean(msgs.get(struct.unpack(">I", e[0x20:0x24])[0], "")) for i, e in enumerate(table_rows(rel, 0x11c7c4, 0x14a6d4, 0x38))}
    _WORLD.update(rel=rel, toc=toc, msgs=msgs, dol=dol, floors=fl, by_arch={f["archive"]: f for f in fl if f["archive"]},
                  by_room={f["room_id"]: f for f in fl}, arenas=world.arenas(rel), battles=world.battles(rel),
                  treasure=world.treasure(rel), items=items, moves=moves, song=music_lookup(disc, rel),
                  species=species_names(disc, rel))
    _WORLD["pkx_species"] = {}
    for i, e in enumerate(table_rows(rel, 0x121a04, 0x14a6e0, 0x11C)):
        pid = struct.unpack(">I", e[0x2C:0x30])[0]
        if pid: _WORLD["pkx_species"].setdefault(pid, _WORLD["species"][i])
    return _WORLD

def placements(disc):
    """Every NPC placement in every room (cached): room modules' character tables (colo/world.py room_characters)."""
    w = world_ctx(disc)
    if "placements" in w: return w["placements"]
    rel = w["rel"]; slots = world.floor_slots(rel); info = world.npc_info(rel); names = world.character_names(rel)
    models = {}
    for arch in ("people_archive", "field_common"):
        for k, n, t, ident, ld in fsys_entries(disc.file(arch + ".fsys")):
            if t == 2: models.setdefault(ident, n)
    out = {}
    for f in w["floors"]:
        arch = f["archive"]; sl = slots.get(f["index"])
        if not arch or not sl or f"{arch}.fsys" not in disc.files: continue
        mod = [ld() for k, n, t, ident, ld in fsys_entries(disc.file(arch + ".fsys")) if t == 14]
        if not mod: continue
        chars = world.room_characters(mod[0], sl.get(0x14))
        for c in chars:
            c["model"] = models.get(info[c["info"]]["model"], f"{info[c['info']]['model']:#x}") if c["info"] < len(info) else "?"
            c["who"] = clean(w["msgs"].get(names[c["name"]]["name_msg"], "")) if c["name"] and c["name"] < len(names) else ""
        tabs = {}
        for off, label in ((0x18, "position points"), (0x1C, "cameras"), (0x10, "lighting entries")):
            t = world.module_table(mod[0], sl.get(off)) if sl.get(off) else None
            if t: tabs[label] = t[0]
        out[arch] = dict(chars=chars, tabs=tabs)
    w["placements"] = out
    return out

def ranges(ids):
    """[1,2,3,7] -> '1-3, 7'"""
    out = []
    for i in sorted(set(ids)):
        if out and i == out[-1][1] + 1: out[-1][1] = i
        else: out.append([i, i])
    return ", ".join(f"{a}" if a == b else f"{a}-{b}" for a, b in out)

def room_details(disc, arch):
    """Facts about one room archive from the floor, battle-floor, battle and treasure tables."""
    w = world_ctx(disc); f = w["by_arch"].get(arch); items = []
    aid = arch_id(arch)
    if aid is not None: items.append((f"archive id {aid:#06x} (FSYS header +0x08; GSfsys.toc maps it to `{arch}`)", V))
    if not f:
        items.append(("no floor-table entry points at this archive: loaded some other way (an event variant or sub-scene) or unused", G))
        return items
    items += [(f"floor table entry {f['index']} (lbl_80478FBC, floorDataBiosGetPtr): **room id {f['room_id']:#x}** (+0x0C; what battles, warps and scripts use), map resource id {f['map_res']:#x} (+0x08)", V),
              (f"class byte {f['cls']:#04x} = {world.FLOOR_CLASS.get(f['cls'], 'unknown')} (meaning inferred from which rooms share it)", G),
              (f"area byte {f['area']} = {world.AREA_NAMES.get(f['area'], 'unknown')} (floorDataBiosGetArea)", V)]
    for a in (a for a in w["arenas"] if a["room_id"] == f["room_id"]):
        name = clean(w["msgs"].get(a["name_msg"], "")) if a["name_msg"] else ""
        bs = [b for b in w["battles"] if b["arena"] == a["index"]]
        song = w["song"](a["music"]) if a["music"] else None
        items.append((f"battle arena {a['index']} (battle-floor table fight_floor_data)" + (f", name message: \"{name}\"" if name else "") + "; "
                      + (f"arena music sound {a['music']} = {song}; " if song else "")
                      + (f"{len(bs)} battle records use it: ids {ranges(b['index'] for b in bs)}" if bs else "no battle record uses it"), V))
        kinds = {}
        for b in bs:
            k = world.BATTLE_TYPE.get(b["type"], f"type {b['type']}"); kinds[k] = kinds.get(k, 0) + 1
        if kinds: items.append(("battle kinds there (battle record +0x00): " + ", ".join(f"{k} x{n}" for k, n in kinds.items()), G))
        own = sorted({w["song"](b["music"]) for b in bs if b["music"] and w["song"](b["music"])})
        if own: items.append(("songs named by those battle records (they take precedence over the arena's, docs/engine/music.md): " + ", ".join(own), V))
    if not arch.endswith("_bf") and f"{arch}_bf" in w["by_arch"]:
        items.append((f"battles here are drawn in its battle-field version `{arch}_bf` (room id {w['by_arch'][arch + '_bf']['room_id']:#x})", V))
    if arch.endswith("_bf") and arch[:-3] in w["by_arch"]:
        items.append((f"battle-field version of `{arch[:-3]}` (room id {w['by_arch'][arch[:-3]]['room_id']:#x})", V))
    pl = placements(disc).get(arch)
    if pl:
        items.append((f"room module tables (prolog -> floor slots): {len(pl['chars'])} characters, " + ", ".join(f"{n} {k}" for k, n in pl["tabs"].items()), V))
        for c in pl["chars"]:
            x, y, z = c["pos"]
            items.append((f"character {c['index']}: " + (f"{c['who']} " if c["who"] else "") + f"model `{c['model']}` (NPC info {c['info']}) at ({x:.1f}, {y:.1f}, {z:.1f})"
                          + (f", talk script {c['talk_script']:#x}" if c["talk_script"] else "") + (f", move script {c['move_script']:#x}" if c["move_script"] else "")
                          + f", flags {c['flags']:#04x} {c['flags2']:#04x}", V))
    for t in w["treasure"]:
        if t["room_id"] == f["room_id"]:
            x, y, z = t["pos"]
            items.append((f"item placed in the room: {w['items'].get(t['item'], t['item'])} x{t['quantity']} (treasure entry {t['index']}, floor_tresure_list; container kind {t['kind']}, "
                          f"position ({x:.1f}, {y:.1f}, {z:.1f}), facing {t['angle']} degrees, collected flag {t['flag']:#x}; layout read from the data, it fits every entry)", G))
    return items

def cmd_pokemon(disc, out):
    rel = common_rel(disc); msgs = all_text(disc); toc = gsfsys_toc(disc)
    names = {i: clean(msgs.get(struct.unpack(">I", e[0x18:0x1C])[0], "")) for i, e in enumerate(table_rows(rel, 0x121a04, 0x14a6e0, 0x11C))}
    faces = table_rows(rel, 0x44, 0x14a65c, 8)
    tex = {}
    for arch in ("poke_face", "poke_body"):
        for k, name, t, ident, load in fsys_entries(disc.file(arch + ".fsys")):
            if t == 9: tex[ident] = (arch, k, name, load)
    cache = {}
    def get(ident):
        if ident not in tex: return None
        if ident not in cache: cache[ident] = decode_texture_file(tex[ident][3]())
        return cache[ident]
    pkx = dol_table(disc, 0x80370BD0, 0x1390 // 12)
    rare = {}
    for a_, b_, c_ in dol_table(disc, 0x80374F20, 0x1A8 // 12):  # {u16 species, u16 0} archive, entry
        if a_ >> 16: rare[a_ >> 16] = (b_, c_)
    base = os.path.join(out, "pokemon")
    readme(base, "One folder per Pokémon species (<internal index>_<Name>): portraits, body pictures, battle model and the textures inside it.",
           "portraits/*.png 42x42; body_pictures/*.png 64x64; model/<Name>.pkx (game file) + <Name>.dat (HSD model for "
           "Blender); textures/*.png (textures inside the model). Index = the game's internal species order, not the "
           "National Dex.", evidence=[("Folder name = the species' English name", "name message id at species record +0x18 (common_rel species table), text from the bank inside common_rel", "verified (code + data)"),
            ("Portraits: normal and shiny cells", "species record face[0]/face[1] -> menu-face record (bit 7 = shiny cell) -> poke_face sheet; drawing code crops 42x42 cells (gs_range_80109C88.c)", "verified (code)"),
            ("Body pictures: _c file = shiny", "species face[1] body id equals the entry id of bodyNNN_c", "verified (data)"),
            ("Battle model", "species pkxDataId -> table lbl_80370BD0 in main.dol {archive id, entry id, name} -> pkx archive", "verified (code: fn_801DE190)"),
            ("model_shiny/ (35 species only)", "shiny battle model: fn_801DE190 loads the pkx_rare_* model from lbl_80374F20 instead of recolouring", "verified (code)"),
            ("Model textures", "image descriptors found inside the HSD model by a heuristic scan (TObj/ImageDesc layout as in Melee)", "guess (heuristic; may miss or add textures)")],
           docs=["docs/engine/textures.md", "docs/engine/models-animation.md", "docs/engine/animation-catalog.md", "docs/engine/pokemon-data.md"],
           checks=[1, 4, 7])
    done = 0
    for sp, e in enumerate(table_rows(rel, 0x121a04, 0x14a6e0, 0x11C)):
        if not names.get(sp): continue
        nm = title(names[sp]); folder = os.path.join(base, f"{sp:03d}_{nm}")
        nat, hoenn, cry = struct.unpack(">H", e[0x12:0x14])[0], struct.unpack(">H", e[0x14:0x16])[0], struct.unpack(">H", e[0x0E:0x10])[0]
        sp_ids = [(f"internal species index {sp} (species table in common_rel, lbl_80478F94 + {sp} x 0x11C; pokemonDataBiosGetPtr)", V),
                  (f"National Pokédex number {nat} (species +0x12), Hoenn Pokédex number {hoenn} (species +0x14)", V),
                  (f"cry sound id {cry} (species +0x0E; sound table snd_data)", G)]
        readme(folder, f"{names[sp].title()} (internal species index {sp}): portraits (normal, shiny), body pictures, battle model (.pkx and .dat) and its textures.",
               "portraits/ PNG 42x42; body_pictures/ PNG 64x64; model/ .pkx (game file) and .dat (HSD model for the Blender GameCube importer), <Name>_animations.csv (the model's animation list: index and length in frames; the PKX slot table points into it), plus <Name>_ambient_particles.gpt1 for the 12 species with an ambient effect; model_shiny/ the separate shiny model (35 species); textures/ PNG",
               evidence=[("Folder name = the species' English name", "name message id at species record +0x18 (common_rel species table), text from the bank inside common_rel", "verified (code + data)"),
            ("Portraits: normal and shiny cells", "species record face[0]/face[1] -> menu-face record (bit 7 = shiny cell) -> poke_face sheet; drawing code crops 42x42 cells (gs_range_80109C88.c)", "verified (code)"),
            ("Body pictures: _c file = shiny", "species face[1] body id equals the entry id of bodyNNN_c", "verified (data)"),
            ("Battle model", "species pkxDataId -> table lbl_80370BD0 in main.dol {archive id, entry id, name} -> pkx archive", "verified (code: fn_801DE190)"),
            ("model_shiny/ (35 species only)", "shiny battle model: fn_801DE190 loads the pkx_rare_* model from lbl_80374F20 instead of recolouring", "verified (code)"),
            ("Model textures", "image descriptors found inside the HSD model by a heuristic scan (TObj/ImageDesc layout as in Melee)", "guess (heuristic; may miss or add textures)")],
               docs=["docs/engine/textures.md", "docs/engine/models-animation.md", "docs/engine/animation-catalog.md"], checks=[1, 4, 7])
        for which, fo in (("normal", 0x10C), ("shiny", 0x114)):
            pid, body = struct.unpack(">HI", e[fo + 2:fo + 8])
            if pid and pid < len(faces):
                rec = faces[pid]; fid = struct.unpack(">I", rec[4:8])[0]; r = get(fid)
                if r:
                    w, h, rgba, gx = r; y = 42 if (rec[0] & 0x80) and h > 42 else 0
                    a, k, en, _ = tex[fid]; pdir = mkdir(os.path.join(folder, "portraits"))
                    p = os.path.join(pdir, f"{nm}_{which}.png"); png(p, 42, 42, crop(rgba, w, 0, y, 42, 42))
                    note(p, a, f"{k} ({en}) cell y={y}", en, "PNG", "42x42", gx)
                    keep_original(pdir, a, f"{k} ({en})", en, 9, tex[fid][3](), p, f"GX {gx}, {w}x{h} sheet of 42x42 cells",
                                  info={"names": [(f"{names[sp].title()} portrait sheet; file name `{en}` (face + number)", V)],
                                        "ids": sp_ids + [(f"portrait id {pid:#x} ({which}) in species face[{0 if which == 'normal' else 1}] (+0x10C / +0x114); menu-face record {pid} in lbl_80478E74 (common_rel), shiny bit {rec[0] >> 7}", V)],
                                        "code": [("_menuFaceBiosGetPtr (0x8010C3FC, gs_colsys.c), shiny-cell bit fn_8010C388, draw fn_8010B9E8 (gs_range_80109C88.c)", V)]})
                    if w > 42:
                        p = os.path.join(pdir, f"{nm}_{which}_facing_other_way.png"); png(p, 42, 42, crop(rgba, w, 42, y, 42, 42))
                        note(p, a, f"{k} ({en}) cell x=42 y={y}", en, "PNG", "42x42", gx)
                        keep_original(pdir, a, f"{k} ({en})", en, 9, tex[fid][3](), p, f"GX {gx}, {w}x{h} sheet of 42x42 cells")
            if body and get(body):
                w, h, rgba, gx = get(body); a, k, en, _ = tex[body]
                bdir = mkdir(os.path.join(folder, "body_pictures"))
                p = os.path.join(bdir, f"{nm}_{which}.png"); png(p, w, h, rgba)
                note(p, a, f"{k} ({en})", en, "PNG", f"{w}x{h}", gx)
                keep_original(bdir, a, f"{k} ({en})", en, 9, tex[body][3](), p, f"GX {gx}, {w}x{h}",
                              info={"names": [(f"{names[sp].title()} body picture ({which}); file `{en}` (`_c` = shiny)", V)],
                                    "ids": sp_ids + [(f"body-picture id {body:#010x} = this entry's id, from species face[{0 if which == 'normal' else 1}] +0x04", V)]})
        if sp in rare:  # separate shiny model (lbl_80374F20; docs/engine/textures.md, "Shiny colouring")
            found = find_entry(disc, rare[sp][0], rare[sp][1], toc)
            if found:
                a, k, en, load = found
                save_model(os.path.join(folder, "model_shiny"), f"{nm}_shiny", load(), a, f"{k} ({en})", en, True,
                           info={"names": [(f"{names[sp].title()} shiny battle model; internal name `{en}` (rare_ = shiny)", V)],
                                 "ids": sp_ids + [(f"shiny-model table lbl_80374F20 (main.dol): species {sp} -> archive {rare[sp][0]:#x}, entry {rare[sp][1]:#010x}; loaded instead of recolouring when shiny (fn_801DE190)", V)]})
        pid = struct.unpack(">I", e[0x2C:0x30])[0]
        if pid and pid < len(pkx):
            found = find_entry(disc, pkx[pid][0], pkx[pid][1], toc)
            if found:
                a, k, en, load = found
                save_model(folder, nm, load(), a, f"{k} ({en})", en, True,
                           info={"names": [(f"{names[sp].title()} battle model; internal name `{en}` (romanized Japanese)", V)],
                                 "ids": sp_ids + [(f"pkxDataId {pid} (species +0x2C) -> Pokémon-model table lbl_80370BD0 + {pid} x 12 (main.dol): archive {pkx[pid][0]:#x}, entry {pkx[pid][1]:#010x}, name message {pkx[pid][2]:#x}", V)]})
        done += 1
    print(f"{done} species -> {base}")

def trainer_model_details(disc, arch, ident):
    """Which trainers (and battles) use one battle model: trainer +0x10 -> lbl_80370840 (fn_801DE418)."""
    w = world_ctx(disc)
    if "trainer_users" not in w:
        models = dol_table(disc, 0x80370840, 0x390 // 12)
        cls = world.trainer_classes(w["rel"]); users = {}
        for t in world.trainers(w["rel"]):
            users.setdefault(t["model"], []).append(t)
        tb = {}
        for b in w["battles"]:
            for tid in b["trainers"]:
                if tid: tb.setdefault(tid, set()).add(b["index"])
        w.update(trainer_models=models, trainer_cls=cls, trainer_users=users, trainer_battles=tb)
    items = []
    for mi, (a, e, nm) in enumerate(w["trainer_models"]):
        if e != ident: continue
        label = clean(w["msgs"].get(nm, ""))
        items.append((f"trainer-model table lbl_80370840 entry {mi} (trainer +0x10 selects it; loaded by fn_801DE418)" + (f"; developer label \"{label}\"" if label else ""), V))
        ts = w["trainer_users"].get(mi, [])
        if not ts: items.append(("no trainer record uses this entry (story / cutscene model, or unused)", G)); continue
        def cname(t):
            c = w["trainer_cls"][t["cls"]] if t["cls"] < len(w["trainer_cls"]) else None
            return clean(w["msgs"].get(c["name_msg"], "")) if c else ""
        names = {}
        for t in ts:
            key = (cname(t) + " " + clean(w["msgs"].get(t["name_msg"], ""))).strip() or f"trainer {t['index']}"
            names.setdefault(key, []).append(t["index"])
        items.append((f"{len(ts)} trainer records use it: " + "; ".join(f"{k} (ids {ranges(v)})" for k, v in list(names.items())[:25])
                      + (f"; ... {len(names) - 25} more names" if len(names) > 25 else ""), V))
        bs = set().union(*[w["trainer_battles"].get(t["index"], set()) for t in ts])
        if bs: items.append((f"battles with these trainers (battle record +0x18 / +0x20 / +0x28 / +0x30): {len(bs)}, ids {ranges(bs)}", G))
    if not items: items.append(("not in the trainer-model table lbl_80370840 (loaded another way, e.g. a cutscene, or unused)", G))
    return items

def cmd_trainers(disc, out):
    msgs = all_text(disc); toc = gsfsys_toc(disc)
    base = os.path.join(out, "trainers")
    readme(base, "One folder per trainer / character battle model, plus character_art/.",
           "model/<name>.pkx + .dat, textures/*.png; folder name = file name and, when known, the class name "
           "from the trainer-model table (lbl_80370840 in main.dol)",
           evidence=[("Trainer / character battle models", "pkx_ archives whose names carry a model suffix (_a1, _m_, _f_); same PKX format as Pokémon", "verified (format), names guess"),
                     ("Class names on folders", "trainer-model table lbl_80370840 in main.dol: {archive, entry, name message}", "guess (table read from data, not yet from code)")],
           docs=["docs/engine/models-animation.md", "docs/engine/animation-catalog.md", "docs/engine/trainers.md"], checks=[5])
    named = {}
    for arch_id, ident, msg in dol_table(disc, 0x80370840, 0x390 // 12):
        if ident: named[ident] = clean(msgs.get(msg, ""))
    seen = set(); n = 0
    for arch, arc in archives(disc):
        if not arch.startswith("pkx_"): continue
        for k, name, t, ident, load in fsys_entries(arc):
            if t != 15 or not is_trainer_pkx(name) or ident in seen: continue
            cls = title(named.get(ident, "")); seen.add(ident)
            label = name + (f"_{cls}" if cls != "Unnamed" else "")
            save_model(os.path.join(base, safe(label)), safe(name), load(), arch, f"{k} ({name})", name, True); n += 1
            detail(os.path.join(base, safe(label)), trainer_model_details(disc, arch, ident))
    art = os.path.join(base, "character_art")
    readme(art, "Character art: large and small trainer / character pictures.", "PNG (from RGBA8)", "chara_big.fsys, chara_small.fsys")
    for arch in ("chara_big", "chara_small"):
        for k, name, t, ident, load in fsys_entries(disc.file(arch + ".fsys")):
            if t == 9: save_texture(os.path.join(mkdir(os.path.join(art, arch)), safe(name)), load(), arch, f"{k} ({name})", name)
    print(f"{n} trainer models -> {base}")

def cmd_npcs(disc, out):
    base = os.path.join(out, "npcs")
    readme(base, "Overworld models of NPCs, the player and Rui (people_archive and field_common), one folder each.",
           "model/<name>.dat (HSD, Blender GameCube importer) + textures/*.png. Names are the game's file names "
           "(romanized Japanese: ippan = ordinary person, m/f = male/female, b1 = overworld model variant 1)",
           evidence=[("Overworld character models", "type-2 entries of people_archive.fsys (people = NPCs) and field_common.fsys; the same names appear in room archives", "verified (archive), roles guess")],
           docs=["docs/engine/fsys-catalog.md", "docs/engine/game-data-tables.md"])
    n = 0
    for arch in ("people_archive", "field_common"):
        for k, name, t, ident, load in fsys_entries(disc.file(arch + ".fsys")):
            if t == 2:
                save_model(os.path.join(base, safe(name)), safe(name), load(), arch, f"{k} ({name})", name, False); n += 1
                where = [(r, c) for r, pl in placements(disc).items() for c in pl["chars"] if c["model"] == name]
                if where:
                    rooms = {}
                    for r, c in where: rooms.setdefault(r, []).append(c["who"] or f"character {c['index']}")
                    detail(os.path.join(base, safe(name)), [(f"placed in {len(rooms)} rooms ({len(where)} placements, from the room modules' character tables): "
                                                             + "; ".join(f"{r}: {', '.join(v)}" for r, v in sorted(rooms.items())), V)])
                else:
                    detail(os.path.join(base, safe(name)), [("not placed by any room's character table (spawned by scripts, the player / partner, or unused)", G)])
    print(f"{n} NPC models -> {base}")

def save_collision(rdir, arch, k, name, data):
    """Room collision (CCD) -> collision/<name>.obj (Wavefront, one object per collision object and group) + summary."""
    cdir = mkdir(os.path.join(rdir, "collision"))
    try:
        objs = collision.parse(data)
    except Exception as e:
        keep_original(cdir, arch, f"{k} ({name})", name, 3, data, [], f"collision, not parsed ({e})"); return
    p = os.path.join(cdir, f"{safe(name)}.obj")
    open(p, "w", encoding="utf-8").write(collision.obj_text(objs, f"{arch} / {name}"))
    note(p, arch, f"{k} ({name})", name, "Wavefront OBJ (collision triangles)")
    lines = []
    for o in objs:
        gs = ", ".join(f"{n} {g['count']}" + (f" (event ids {sorted({t.get('id') for t in g['triangles']})})" if n.endswith("events") else "") for n, g in o["groups"].items())
        moved = o["translate"] != (0.0, 0.0, 0.0) or o["rotate"] != (0.0, 0.0, 0.0) or o["scale"] != (1.0, 1.0, 1.0)
        lines.append((f"object {o['index']}: {gs}" + (" (transform " + ", ".join(f"{x:g}" for x in o['translate'] + o['rotate'] + o['scale']) + ")" if moved else "")
                      + (" (moving object: flag bit 0, queried through its own matrix)" if o["flags"] & 1 else ""), V))
    readme(cdir, f"Collision of room {arch}: {len(objs)} collision objects, exported as one OBJ ({safe(name)}.obj) with one named object per collision object and triangle group.",
           "Wavefront OBJ (open in Blender next to ../map/model/*.dat); object names objNN_<group>[_moving]: floor = walkable floor, walls_player = walls for the player and partner, walls_npc = walls for NPCs, pass_events = walk-through triggers, check_events = examine (A button) planes, group_38 = unknown (docs/engine/collision.md)",
           f"{arch}.fsys", evidence=[("Record and group layout", "pointer fix-up _offsetCCD__FP12CCD_FILEHEAD and the debug drawer GScolsys2Draw / fn_8010D20C (src/game/gs_range_8010CBD0.c)", "verified (code)"),
                                     ("What each group is for", "which query reads each record field: walk / layer queries (+0x24), colsysWallAll for the player (+0x28) and NPCs (+0x34), colsysPassGetEventID (+0x2C), GScolsys2Check (+0x30)", "verified (code); +0x38 unknown")],
           docs=["docs/engine/collision.md"])
    detail(cdir, lines)
    keep_original(cdir, arch, f"{k} ({name})", name, 3, data, [p], f"{len(objs)} collision objects",
                  info={"names": [(f"collision of `{arch}` (CCD file; loader floorReadColPostFunc -> fn_8010CFE4)", V)]})

def save_camera(rdir, arch, k, name, data):
    """Room camera shot (HSD archive with scene_data) -> cameras/<name>.txt summary (colo/hsdcam.py) + original."""
    cdir = mkdir(os.path.join(rdir, "cameras"))
    try:
        cams = hsdcam.cameras(data)
    except Exception as e:
        cams = []
    p = os.path.join(cdir, f"{safe(name)}.txt")
    with open(p, "w", encoding="utf-8") as f:
        f.write(f"; camera shot `{name}` of {arch}.fsys (entry {k}); HSD scene_data camera (docs/engine/floor-resources.md)\n")
        f.write("; keyframes decoded by colo/fobj.py: per key the animation frame, interpolation op (CON / LIN / SPL0 / SPL / SLP / KEY), value and slope (docs/engine/animation-tracks.md)\n")
        for c in cams: f.write(hsdcam.summary(c) + "\n")
    note(p, arch, f"{k} ({name})", name, "camera summary (text)")
    # register on every run (an "only if no README yet" check made re-runs fall back to a generic room description:
    # found by the README comparison, 2026-10-16)
    if os.path.abspath(cdir) not in READMES:
        readme(cdir, f"Camera shots of room {arch}: one text summary per shot (projection, FOV, eye and interest positions, animation tracks).",
               "text summaries; originals (.cam = HSD archive) in original/", f"{arch}.fsys",
               evidence=[("Format", "floorReadCameraPostFunc parses the HSD archive and uses scene_data +0x04 (camera list); layouts as in Melee's sysdolphin", "verified (code)"),
                         ("Track names", "Melee's CObjUpdateFunc / WObjUpdateFunc track numbers (same HSD library)", "verified (Melee source)")],
               docs=["docs/engine/floor-resources.md"])
    keep_original(cdir, arch, f"{k} ({name})", name, 12, data, [p],
                  info={"names": [(f"camera shot `{name}` of `{arch}` (cam_<room>_shot_NN naming)", V)]})

def save_particles(folder, arch, entry, name, data, ftype=10, keep=True, base=0, label=None):
    """A GPT1 particle bank (colo/gpt1.py, docs/engine/particles.md): <folder>/particles/<label>.txt (every emitter's
    settings and byte-code as readable text) and <label>_textures/ (each texture group's images as PNG). Returns the
    files written."""
    from colo import gpt1
    label = label or safe(name)
    pdir = mkdir(os.path.join(folder, "particles"))
    bank = gpt1.parse_bank(data, base)
    txt = os.path.join(pdir, f"{label}.txt")
    open(txt, "w", encoding="utf-8").write(gpt1.listing(data, bank, name) + "\n")
    note(txt, arch, entry, name, "text: GPT1 particle bank listing (emitters, settings, byte-code)")
    files = [txt]
    for g in bank["tex_groups"]:
        if not g: continue
        for i, img in enumerate(gpt1.decode_images(data, bank, g)):
            if not img: continue
            w, h, rgba = img
            p = os.path.join(mkdir(os.path.join(pdir, f"{label}_textures")),
                             f"g{g['index']:02d}_{i:02d}_{w}x{h}_{gpt1.GX_NAMES.get(g['fmt'], g['fmt'])}.png")
            png(p, w, h, rgba); note(p, arch, entry, name, "PNG (particle texture)", f"{w}x{h}", gpt1.GX_NAMES.get(g["fmt"], ""))
            files.append(p)
    if keep: keep_original(pdir, arch, entry, name, ftype, data, files, ext="gpt1")
    return files

def save_wzx(folder, arch, entry, name, data):
    """A WZX move effect (colo/wzx.py, docs/engine/wzx.md): <name>.txt (the timeline) and <name>_parts/: each particle
    bank as a GPT1 listing + textures, each model / resource archive as an HSD .dat, each effect texture as PNG."""
    from colo import wzx
    w = wzx.parse_wzx(data)
    txt = os.path.join(folder, f"{safe(name)}.txt")
    open(txt, "w", encoding="utf-8").write(wzx.listing(w, name) + "\n")
    note(txt, arch, entry, name, "text: WZX move-effect timeline")
    files = [txt]; parts = os.path.join(folder, f"{safe(name)}_parts")
    def blob(label, off, size, fmt):
        p = os.path.join(mkdir(parts), label); open(p, "wb").write(data[off:off + size]); note(p, arch, entry, name, fmt); files.append(p)
    if w["resource"]: blob("resource.dat", *w["resource"], "HSD archive (the move's own resource, loaded with the sequence)")
    for e in w["entries"]:
        p = e["payload"]; i = e["index"]
        if e["type"] == 2: blob(f"e{i:02d}_model.dat", *p["model"], "HSD model (DAT)")
        elif e["type"] == 3 and "bank" in p:
            files += save_particles(parts, arch, entry, name, data, keep=False, base=p["bank"][0], label=f"e{i:02d}_particles")
        elif e["type"] == 4:
            if p.get("archive"): blob(f"e{i:02d}_effect_model.dat", *p["archive"], "HSD archive (screen-effect model)")
            if p.get("texture"):
                off, size = p["texture"]; dec = decode_texture_file(data[off:off + size])
                if dec:
                    tw, th, rgba, gxn = dec; q = os.path.join(mkdir(parts), f"e{i:02d}_effect_texture_{tw}x{th}_{gxn}.png")
                    png(q, tw, th, rgba); note(q, arch, entry, name, "PNG (screen-effect texture)", f"{tw}x{th}", gxn); files.append(q)
    keep_original(folder, arch, entry, name, 16, data, files)
    return files

def save_flare(folder, arch, entry, name, data):
    """GFL lens flare (colo/flare.py, docs/engine/lens-flare.md): <folder>/lens_flare/<name>.txt + texture PNGs."""
    from colo import flare
    f = flare.parse_flare(data)
    fdir = mkdir(os.path.join(folder, "lens_flare")); files = []
    if f:
        p = os.path.join(fdir, f"{safe(name)}.txt"); open(p, "w", encoding="utf-8").write(flare.listing(f, name) + "\n")
        note(p, arch, entry, name, "text: lens-flare settings and elements"); files.append(p)
        for t in f["textures"]:
            if not t["image"]: continue
            w, h, rgba, gxn = t["image"]; p = os.path.join(fdir, f"{safe(name)}_tex{t['id']}_{w}x{h}_{gxn}.png"); png(p, w, h, rgba)
            note(p, arch, entry, name, "PNG (lens-flare texture)", f"{w}x{h}", gxn); files.append(p)
    keep_original(fdir, arch, entry, name, 17, data, files)

_SLOTS = {}
def save_room_data(disc, rdir, arch, arc):
    """The room module's position points, field-camera points and lighting record as text (colo/world.py;
    docs/engine/game-data-tables.md, "Per-room modules")."""
    w = world_ctx(disc)
    if not _SLOTS: _SLOTS.update(world.floor_slots(w["rel"]))
    f = w["by_arch"].get(arch); sl = _SLOTS.get(f["index"]) if f else None
    mod = [ld() for k, n, t, ident, ld in fsys_entries(arc) if t == 14]
    if not sl or not mod: return
    m = mod[0]; L = [f"# {arch}: room module records", ""]
    pos = world.room_positions(m, sl.get(0x18)) if sl.get(0x18) else []
    L += [f"## Position points ({len(pos)}): where the player appears on entering (index = the entry point)", ""]
    L += [f"- {p['index']}: ({p['x']:.2f}, {p['y']:.2f}, {p['z']:.2f}), facing {p['direction']} degrees" for p in pos]
    cams = world.room_cameras(m, sl.get(0x1C)) if sl.get(0x1C) else []
    L += ["", f"## Field-camera points ({len(cams)}): blended by distance from the player", ""]
    L += [f"- {c['index']}: anchor ({c['anchor'][0]:.2f}, {c['anchor'][1]:.2f}, {c['anchor'][2]:.2f}), distance {c['distance']:.2f}, "
          f"height {c['height']:.2f}, Y rotation {c['rot_y']:.3f} rad" for c in cams]
    li = world.room_lighting(m, sl.get(0x10)) if sl.get(0x10) else None
    if li:
        def sub(r):  # map sub-resources (floorReadMapPostFunc): 0x1000 + n model set n, 0x1600 + n light n, 0x1800 camera
            low = r & 0xFFFF
            return (f"map model set {low - 0x1000}" if 0x1000 <= low < 0x1600 else f"map light {low - 0x1600}" if 0x1600 <= low < 0x1800
                    else f"{r:#x}") + f" ({r:#x})"
        L += ["", "## Lighting", "",
              "- sun: " + (sub(li["sun"]) + " (its first part's position aims the lens flare)" if li["sun"] else "none"),
              "- shadow light: " + (sub(li["shadow_light"]) if li["shadow_light"] else "none"),
              "- shadow receivers (surfaces battle Pokemon cast shadows on): " + (", ".join(sub(r) for r in li["receivers"]) or "none")]
    tvs = world.room_tvs(m, sl.get(0x48)) if sl.get(0x48) else []
    if tvs:
        L += ["", f"## TV screens ({len(tvs)} channel entries; floor entry +0x48)", ""]
        L += [f"- channel {t['channel']}: {t['width']}x{t['height']} texture on screen model {t['screen']:#x}, showing model "
              f"{t['content']:#x} animation {t['anim']} through camera {t['camera']:#x}" for t in tvs]
    gift = world.room_gift_table(m)
    if gift:
        tot = sum(w_ for i_, w_ in gift) or 1
        L += ["", f"## Random item gift ({len(gift)} items; script native std087_sequenceUnk87: first visit, then every 500 steps)", ""]
        L += [f"- {w['items'].get(i_, i_)}: weight {w_} ({100 * w_ / tot:.1f}%)" for i_, w_ in gift]
    p = os.path.join(rdir, f"{arch}_room_data.txt"); open(p, "w", encoding="utf-8").write(chr(10).join(L) + chr(10))
    note(p, arch, "room module (type 14)", arch, "text: room module position, field-camera and lighting records")

def cmd_maps(disc, out):
    base = os.path.join(out, "maps")
    readme(base, "Every room (map) archive, grouped by area (<prefix>_<Area>/<room>/).",
           "map/ = map model (.map.dat, HSD) + its textures; objects/<name>/ = object and NPC models in the room; "
           "collision, script, camera, particles, rel = raw game formats; room text is in ../text/. "
           "Rooms ending _bf are battle-field versions (the stage for battles there).",
           evidence=[("Area names", "room prefixes S1..T1; confirmed by snatcherLeo from TCRF and gc-forever; the floor table's area byte agrees", "verified (snatcherLeo), T1 guess"),
                     ("_bf = battle-field room", "the battle-floor table points every overworld battle at a _bf room", "verified (data)")],
           docs=["docs/engine/fsys-catalog.md", "docs/engine/battle-arenas.md", "docs/engine/game-data-tables.md"], checks=[2, 3])
    n = 0
    for arch, arc in archives(disc):
        m = re.match(r"([A-Z]\d)_", arch)
        if not m: continue
        adir = os.path.join(base, f"{m.group(1)}_{AREAS.get(m.group(1), 'Unknown')}")
        if not os.path.exists(os.path.join(adir, "README.txt")):
            readme(adir, f"Rooms of {AREAS.get(m.group(1), 'an unknown area').replace('_', ' ')} (prefix {m.group(1)}).", "one folder per room")
        rdir = mkdir(os.path.join(adir, arch))
        readme(rdir, f"Room {arch}" + (" (battle-field version)" if arch.endswith("_bf") else ""),
               "map/ (map model + textures), objects/, raw room data files", f"{arch}.fsys")
        detail(rdir, room_details(disc, arch))
        save_room_data(disc, rdir, arch, arc)
        songs = room_script_songs(disc, arc)
        if songs: detail(rdir, [("songs its script starts: " + ", ".join(songs) + f" (listing: scripts/{os.path.basename(adir)}/{arch}__*.txt)", V)])
        for k, name, t, ident, load in fsys_entries(arc):
            if t == 5: continue
            data = load(); n += 1
            if t == 1: save_model(os.path.join(rdir, "map"), safe(name), data, arch, f"{k} ({name})", name, False, 1)
            elif t == 2:
                save_model(os.path.join(rdir, "objects", safe(name)), safe(name), data, arch, f"{k} ({name})", name, False)
                object_details.room = arch
                detail(os.path.join(rdir, "objects", safe(name)), object_details(disc, name))
            elif t == 9: save_texture(os.path.join(mkdir(os.path.join(rdir, "textures")), safe(name)), data, arch, f"{k} ({name})", name)
            elif t == 3: save_collision(rdir, arch, k, name, data)
            elif t == 12: save_camera(rdir, arch, k, name, data)
            elif t in (10, 18) and data[:4] == b"GPT1": save_particles(rdir, arch, f"{k} ({name})", name, data, t)
            elif t == 17: save_flare(rdir, arch, f"{k} ({name})", name, data)
            else:
                keep_original(rdir, arch, f"{k} ({name})", name, t, data, [], "not converted yet")
    print(f"{n} room entries -> {base}")

_ARCH_DONE = set()
def archive_details(disc, arch):
    """What a non-room archive is for (folder_notes.ARCHIVE_PURPOSE) or, for a room archive, its room facts."""
    if re.match(r"[A-Z]\d_", arch): return [(f"room archive `{arch}`: see extracted/maps/ for the room's details", V)] + room_details(disc, arch)
    aid = arch_id(arch)
    return ([(f"archive id {aid:#06x} (FSYS header +0x08)", V)] if aid is not None else []) + list(ARCHIVE_PURPOSE.get(arch, [(f"purpose of `{arch}` not recorded yet (docs/engine/fsys-catalog.md)", G)]))

def cmd_ui(disc, out):
    base = os.path.join(out, "ui")
    readme(base, "Menu, UI, logo and screen textures, one folder per menu archive.",
           "PNG RGBA, decoded from GameCube RGBA8 / RGB5A3 / RGB565 / CMPR (docs/engine/textures.md)",
           evidence=[("Menu and screen textures", "type-9 entries of menu archives (names like menu_*, logos, title)", "verified (format), screen per texture guess")],
           docs=["docs/engine/textures.md", "docs/engine/fsys-catalog.md"])
    seen = set(); n = 0
    for arch, arc in archives(disc):
        if arch in ("poke_face", "poke_body") or arch.startswith(("chara_", "pkx_", "wzx_")) or re.match(r"[A-Z]\d_", arch): continue
        for k, name, t, ident, load in fsys_entries(arc):
            if t != 9 or ident in seen: continue
            seen.add(ident); save_texture(os.path.join(mkdir(os.path.join(base, safe(arch))), safe(name)), load(), arch, f"{k} ({name})", name); n += 1
            if arch not in _ARCH_DONE: _ARCH_DONE.add(arch); detail(os.path.join(base, safe(arch)), archive_details(disc, arch))
    print(f"{n} UI textures -> {base}")

def cmd_effects(disc, out):
    base = os.path.join(out, "effects")
    readme(base, "Move effects (moves/<move>/<part>.wzx) and shared particle loops (global/).",
           "<part>.txt = the effect's timeline (every entry: start, type, settings); <part>_parts/ = its particle banks "
           "(GPT1 listing + textures), models (.dat for Blender) and effect textures (PNG); original/ = the .wzx files. "
           "Formats: docs/engine/wzx.md, particles.md; move names are romanized Japanese",
           evidence=[("Move effect sequences", "type-16 entries loaded by floorReadWZXPreFunc; parts (attack, damage...) from the file names", "verified (type), part meanings guess")],
           docs=["docs/engine/models-animation.md", "docs/engine/fsys-catalog.md"])
    n = 0; w = world_ctx(disc); uses = world.move_effects(w["dol"], w["toc"]); folders = {}
    for arch, arc in archives(disc):
        if not arch.startswith("wzx_") and arch != "waza_sequence_global": continue
        for k, name, t, ident, load in fsys_entries(arc):
            if arch == "waza_sequence_global": folder = mkdir(os.path.join(base, "global"))
            else:
                mv = re.sub(r"_(attack|damage\d*|special|sp\d|status|out|downin|in|open|all)$", "", name)
                folder = mkdir(os.path.join(base, "moves", safe(mv)))
            data = load(); n += 1
            folders.setdefault(folder, set()).add(arch)  # before the converters: the "In the game data" details need it
            if t in (10, 18) and data[:4] == b"GPT1": save_particles(folder, arch, f"{k} ({name})", name, data, t); continue
            if t == 16: save_wzx(folder, arch, f"{k} ({name})", name, data); continue
            keep_original(folder, arch, f"{k} ({name})", name, t, data, [], "not converted yet")
    for folder, archs in folders.items():
        lines = []
        for arch in sorted(archs):
            for how, ident, det in uses.get(arch, []):
                if how == "move":
                    det = re.sub(r"Pokémon model (\d+)", lambda m_: f"Pokémon model {m_.group(1)} = {w['pkx_species'].get(int(m_.group(1)), '?')}", det)
                    lines.append((f"`{arch}`: move {ident} {w['moves'].get(ident, '?')}, {det}", V))
                else:
                    det = re.sub(r"name message (0x[0-9a-f]+)", lambda m_: f"name message {m_.group(1)}" + (f" = \"{clean(w['msgs'].get(int(m_.group(1), 16), ''))}\"" if clean(w['msgs'].get(int(m_.group(1), 16), '')) else ""), det)
                    lines.append((f"`{arch}`: {det}", V))
            if arch not in uses and arch != "waza_sequence_global":
                lines.append((f"`{arch}`: not in the move-effect tables (lbl_8036E150 / lbl_80373210 / lbl_80373750 / lbl_803727C8); loaded another way or unused", G))
        detail(folder, lines)
    print(f"{n} effect files -> {base}")

def cmd_text(disc, out):
    d = os.path.join(out, "text")
    readme(d, "All game text: every message bank, one file per bank.",
           "UTF-8 text, one line per message: <message id> TAB <text>; control codes shown as <XX>, or <XX:hex> with their argument bytes (colo/msg.py CONTROL; docs/engine/messages.md)",
           evidence=[("Message banks", "type-5 entries (floorReadMsg*) and the three banks in common_rel's resource table", "verified (code + data)"),
                     ("Text encoding UTF-16", "English and Japanese text decode as Unicode", "verified (data)")],
           docs=["docs/engine/messages.md"], checks=[6])
    seen = set(); n = 0
    def write(path, data, arch, entry, original, keep=True):
        with open(path, "w", encoding="utf-8", errors="backslashreplace") as f:
            for mid, s in bank_text(data): f.write(f"{mid:#07x}\t{s}\n")
        note(path, arch, entry, original, "UTF-8 text (message bank)")
        if keep: keep_original(d, arch, entry, f"{arch}__{original}", 5, data, path)
    rel = common_rel(disc)
    for name, off in REL_BANKS: write(os.path.join(mkdir(d), f"{name}.txt"), rel[SEC4 + off:], "common", f"common_rel section 4 +{off:#x}", "common_rel", keep=False)  # original: inside data_tables/original/common_rel.rel
    for arch, arc in archives(disc):
        for k, name, t, ident, load in fsys_entries(arc):
            if t != 5 or ident in seen: continue
            seen.add(ident); write(os.path.join(d, f"{safe(arch)}__{safe(name)}.txt"), load(), arch, f"{k} ({name})", name); n += 1
    print(f"{n + 3} message banks -> {d}")

def cmd_tables(disc, out):
    rel = common_rel(disc); d = mkdir(os.path.join(out, "data_tables"))
    readme(d, "The game's data tables (species, moves, trainers, trainer Pokémon, battles, floors, treasure, flags, ...).",
           "CSV, one row per entry, raw 32-bit words in hex (columns w00, w04, ...); species.json with the decoded "
           "fields known so far. Field meanings: docs/engine/game-data-tables.md, pokemon-data.md, trainers.md.",
           "common.fsys -> common_rel (REL data module, section 4)",
           evidence=[("Table pointers and counts", "common_rel's prolog writes each table's pointer and count into main.dol globals (relocations)", "verified (code + data)"),
                     ("Species, moves, trainers, trainer Pokémon, flags, Shadow sizes", "entry sizes agree with the accessor code", "verified (code)"),
                     ("Other field meanings", "accessor names", "partly guess (see docs)")],
           docs=["docs/engine/game-data-tables.md", "docs/engine/pokemon-data.md", "docs/engine/trainers.md", "docs/engine/battle-arenas.md"])
    for name, off, cnt, size in TABLES:
        p = os.path.join(d, f"{name}.csv")
        with open(p, "w", encoding="utf-8") as f:
            f.write("index," + ",".join(f"w{4 * j:02X}" for j in range(size // 4)) + "\n")
            for i, r in enumerate(table_rows(rel, off, cnt, size)):
                f.write(f"{i}," + ",".join(f"{struct.unpack('>I', r[4 * j:4 * j + 4])[0]:08X}" for j in range(size // 4)) + "\n")
        note(p, "common", f"common_rel section 4 +{off:#x}", name, "CSV")
    keep_original(d, "common", "0 (common_rel)", "common_rel", 14, rel, [os.path.join(d, f"{t[0]}.csv") for t in TABLES])
    keep_original(d, "main.dol", "(disc header +0x420)", "main", 0, disc.dol(), [os.path.join(d, "items_decoded.csv")],
                  "the game executable (items, Pokémon-model, trainer-model and shiny-model tables)", ext="dol")
    names = species_names(disc, rel)
    # index = the internal number (what the game stores); nationalDex = +0x12 (numPokemon), hoennFirst = +0x14
    # (numZukan: Hoenn's order, then the rest): pokemon-data.md "Every species has three numbers"
    species = [{"index": i, "name": names[i], "nationalDex": struct.unpack(">H", e[0x12:0x14])[0],
                "hoennFirst": struct.unpack(">H", e[0x14:0x16])[0], "pkxDataId": struct.unpack(">I", e[0x2C:0x30])[0],
                "types": list(e[0x30:0x32]), "abilities": list(e[0x32:0x34]),
                "baseStats": list(struct.unpack(">6H", e[0x84:0x90])),
                "faces": [list(struct.unpack(">BxHI", e[o:o + 8])) for o in (0x10C, 0x114)]}
               for i, e in enumerate(table_rows(rel, 0x121a04, 0x14a6e0, 0x11C))]
    p = os.path.join(d, "species.json")
    json.dump(species, open(p, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    note(p, "common", "common_rel section 4 +0x121a04", "species", "JSON")
    msgs = all_text(disc)
    def txt(mid): return clean(msgs.get(mid, "")).replace('"', "'")
    def csvrow(f, vals): f.write(",".join('"' + str(v) + '"' for v in vals) + "\n")
    # moves: decoded (layout: docs/engine/pokemon-data.md, "Move entry")
    p = os.path.join(d, "moves_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        # effect_id = +0x1A (SeqId: the move-effect / battle-script id, Gen 3's 0-213; corrected 2026-10-16:
        # it used to be read from +0x18, which is 0 in every move and is now the unknown_18 column)
        csvrow(f, ["index", "name", "type", "power", "accuracy", "pp", "priority", "effect_id", "effect_chance",
                   "target_range", "contact", "protect", "sound", "unknown_18", "wzx_id", "description"])
        for i, e in enumerate(table_rows(rel, 0x11c7c4, 0x14a6d4, 0x38)):
            csvrow(f, [i, txt(struct.unpack(">I", e[0x20:0x24])[0]), e[2], struct.unpack(">h", e[0x16:0x18])[0], e[4], e[1],
                       struct.unpack(">b", e[0:1])[0], struct.unpack(">H", e[0x1A:0x1C])[0], e[5], e[3], e[6], e[7], e[0x10],
                       struct.unpack(">H", e[0x18:0x1A])[0], struct.unpack(">I", e[0x30:0x34])[0],
                       txt(struct.unpack(">I", e[0x2C:0x30])[0])])
    note(p, "common", "common_rel section 4 +0x11c7c4", "moves", "CSV (decoded fields)")
    # reverb presets: 12 x 24 bytes, six floats in the order _sndSetReverbParm (gs_dvd.c) copies them into MusyX's
    # high-quality reverb (docs/engine/music.md, "Sound ids, the sound table and the song table"; found)
    p = os.path.join(d, "reverb_presets_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["preset", "coloration", "mix", "time", "damping", "pre_delay", "crosstalk"])
        for i, e in enumerate(table_rows(rel, 0x1437F8, 0x14A6F0, 24)):
            csvrow(f, [i] + [f"{v:g}" for v in struct.unpack(">6f", e)])
    note(p, "common", "common_rel section 4 +0x1437f8", "reverb presets", "CSV (decoded fields)")
    # items: in main.dol (lookup lbl_803681E8 -> records lbl_80363CE8), docs/engine/game-data-tables.md
    dol = disc.dol()
    offs = struct.unpack(">18I", dol[0:72]); addrs = struct.unpack(">18I", dol[72:144]); sizes = struct.unpack(">18I", dol[144:216])
    def rd(addr, n):
        for O, A, S in zip(offs, addrs, sizes):
            if A <= addr < A + S: return dol[O + addr - A:O + addr - A + n]
    nid = struct.unpack(">I", rd(0x80478BD8, 4))[0]; nrec = struct.unpack(">I", rd(0x80478BB0, 4))[0]
    lookup = struct.unpack(f">{nid}H", rd(0x803681E8, 2 * nid))
    p = os.path.join(d, "items_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["item_id", "record", "name", "kind", "key_item", "price", "coupon_price", "held_effect_id",
                   "battle_effect_id", "description"])
        for iid, slot in enumerate(lookup):
            if slot >= nrec: continue
            r = rd(0x80363CE8 + slot * 0x28, 0x28)
            csvrow(f, [iid, slot, txt(struct.unpack(">I", r[0x10:0x14])[0]), r[0], r[1], struct.unpack(">H", r[6:8])[0],
                       struct.unpack(">H", r[8:10])[0], struct.unpack(">H", r[10:12])[0], struct.unpack(">H", r[12:14])[0],
                       txt(struct.unpack(">I", r[0x14:0x18])[0])])
    note(p, "", "main.dol lbl_80363CE8 via lbl_803681E8", "items", "CSV (decoded fields)")
    # trainers and their Pokemon (layouts: docs/engine/trainers.md, verified from fightTrainerGetStatus and the
    # trainer-Pokemon getters), with names resolved
    item_names = {iid: txt(struct.unpack(">I", rd(0x80363CE8 + slot * 0x28 + 0x10, 4))[0]) for iid, slot in enumerate(lookup) if slot < nrec}
    move_names = {i: txt(struct.unpack(">I", e[0x20:0x24])[0]) for i, e in enumerate(table_rows(rel, 0x11c7c4, 0x14a6d4, 0x38))}
    classes = {i: txt(struct.unpack(">I", e[4:8])[0]) for i, e in enumerate(table_rows(rel, 0x8f724, 0x14a684, 12))}
    tp = table_rows(rel, 0x9e5dc, 0x14a6b4, 0x50)
    p = os.path.join(d, "trainers_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        # party = the 6 trainer-Pokemon entries from +0x04, unused slots species 0 (likely: the data reads this way;
        # +0x06 is listed as field_06 because its values, 0x51-0x54 for story trainers, are not party sizes)
        csvrow(f, ["trainer", "class_id", "class", "name", "sex", "field_06", "first_trainer_pokemon", "model_index", "field_0C", "party"])
        for i, e in enumerate(table_rows(rel, 0x91684, 0x14a6ac, 0x34)):
            kind, first, size = struct.unpack(">HHH", e[2:8])
            party = []
            for k in range(first, first + 6):
                if k < len(tp) and struct.unpack(">H", tp[k][0x0A:0x0C])[0] == 0: continue
                if k < len(tp):
                    sp, lv = struct.unpack(">H", tp[k][0x0A:0x0C])[0], tp[k][4]
                    party.append(f"{names.get(sp, sp)} L{lv}" + (" (Shadow)" if tp[k][3] else ""))
            csvrow(f, [i, kind, classes.get(kind, ""), txt(struct.unpack(">I", e[8:12])[0]), e[0], size, first,
                       struct.unpack(">I", e[0x10:0x14])[0], f"{struct.unpack('>I', e[0x0C:0x10])[0]:#x}", "; ".join(party)])
    note(p, "common", "common_rel section 4 +0x91684", "trainers", "CSV (decoded fields)")
    p = os.path.join(d, "trainer_pokemon_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["index", "species", "level", "shadow_id", "ability_slot", "gender", "nature", "friendship", "ball", "held_item",
                   "ivs", "evs", "moves"])
        for i, e in enumerate(tp):
            sb = struct.unpack(">b", e[0:1])[0], struct.unpack(">b", e[1:2])[0], struct.unpack(">b", e[2:3])[0]
            sp, ball = struct.unpack(">HH", e[0x0A:0x0E]); held = struct.unpack(">i", e[0x10:0x14])[0]
            mv = [struct.unpack(">I", e[0x34 + 8 * k:0x38 + 8 * k])[0] for k in range(4)]  # slot = {u32 (0 or -1), u32 move}
            csvrow(f, [i, names.get(sp, sp), e[4], e[3], sb[0], sb[1], sb[2], struct.unpack(">h", e[8:10])[0],
                       item_names.get(ball, ball), item_names.get(held, held) if held > 0 else "",
                       "/".join(str(v) for v in struct.unpack(">6b", e[0x1C:0x22])), "/".join(str(v) for v in struct.unpack(">6h", e[0x22:0x2E])),
                       "; ".join(move_names.get(m, str(m)) for m in mv if m)])
    note(p, "common", "common_rel section 4 +0x9e5dc", "trainer_pokemon", "CSV (decoded fields)")
    # battles, arenas and treasure (colo/world.py; docs/engine/battle-arenas.md, music.md, game-data-tables.md)
    song = music_lookup(disc, rel); fl = world.floors(rel, gsfsys_toc(disc)); room_arch = {f["room_id"]: f["archive"] or "" for f in fl}
    tr_rows = table_rows(rel, 0x91684, 0x14a6ac, 0x34)
    def tname(t):
        if not t or t >= len(tr_rows): return ""
        e = tr_rows[t]; return (classes.get(struct.unpack(">H", e[2:4])[0], "") + " " + txt(struct.unpack(">I", e[8:12])[0])).strip()
    ars = world.arenas(rel)
    p = os.path.join(d, "arenas_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["arena", "name", "terrain", "room", "room_id", "intro_wzx_id", "music_sound_id", "music_song", "ambient_sound_id"])
        for a_, e in zip(ars, table_rows(rel, 0x8f974, 0x14a68c, 24)):
            csvrow(f, [a_["index"], txt(a_["name_msg"]), a_["kind"], room_arch.get(a_["room_id"], ""), a_["room_id"],
                       f"{struct.unpack('>I', e[8:12])[0]:#x}", a_["music"], song(a_["music"]) or "", struct.unpack(">I", e[0x10:0x14])[0]])
    note(p, "common", "common_rel section 4 +0x8f974", "battle floors", "CSV (decoded fields)")
    p = os.path.join(d, "battles_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["battle", "type", "arena", "arena_name", "music_sound_id", "music_song", "side_trainers"])
        for b_ in world.battles(rel):
            an = txt(ars[b_["arena"]]["name_msg"]) if b_["arena"] < len(ars) else ""
            csvrow(f, [b_["index"], b_["type"], b_["arena"], an, b_["music"], song(b_["music"]) or "",
                       "; ".join(f"{t}: {tname(t)}" for t in b_["trainers"] if t)])
    note(p, "common", "common_rel section 4 +0x109fbc", "battles", "CSV (decoded fields)")
    p = os.path.join(d, "treasure_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["index", "room", "room_id", "item", "quantity", "container_kind", "facing", "flag", "x", "y", "z"])
        for t_ in world.treasure(rel):
            csvrow(f, [t_.get("index", ""), room_arch.get(t_.get("room_id"), ""), t_.get("room_id"), item_names.get(t_.get("item"), t_.get("item")),
                       t_.get("quantity"), t_.get("kind"), t_.get("angle"), t_.get("flag"), *[f"{v:.2f}" for v in t_.get("pos", (0, 0, 0))]])
    note(p, "common", "common_rel section 4 +0x11bfac", "treasure", "CSV (decoded fields)")
    # battle kinds (0x20 bytes: 25 u8 rule flags, pad, u32 name message; FightKindData in src/game/fight.c, field names from
    # the developers' own getters fightKindDataBiosGet*Flag)
    kind_fields = ["backSaveData", "doBadgeCheck", "doZukanMita", "doZukanTukamaeta", "useItem", "call", "nigeru", "draw",
                   "getExp", "getOkane", "okanePool", "getNekoniKoban", "getFriend", "getInfectPokerus", "doCriticalAttack",
                   "doHizukiAi", "doHizukiMiyaburi", "doItemSoubiTokukoutokubouup", "keikentihueru", "boss", "dorobou",
                   "monohiroi", "darkpokemonHypermode", "pokemonStatusMenuSubbar", "hostEnemyMsg"]
    p = os.path.join(d, "battle_kinds_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["kind", "name"] + kind_fields)
        for i, e in enumerate(table_rows(rel, 0x90f4c, 0x14a694, 0x20)):
            csvrow(f, [i, txt(struct.unpack(">I", e[0x1C:0x20])[0])] + list(e[:25]))
    note(p, "common", "common_rel section 4 +0x90f4c", "battle kinds", "CSV (decoded fields)")
    # trainer AI (0x28 bytes; fightTrainerAiDataBiosGet* in src/game/fight_trainer_db_range_801FBD10.c): u8 fields
    # +0x00..+0x19 named by the developers' getters, +0x1A..+0x1D unnamed, +0x1E u16 (unnamed; rises with the index),
    # +0x20 two {u8 type id, u8 value} "zokusei revise" pairs, +0x24 two {u8 type id, u8 value} "waza type revise" pairs
    ai_fields = ["pokemonSelectRandom", "pokemonSelectWeakPoint", "pokemonDataOrderOut", "pokemonDataOrderAceBoss",
                 "keyPlayer", "pokemonJoutai", "comboValue", "irekaeValue", "itemValue", "paramExpect", "paramStore",
                 "zokuseiCheck", "tokuseiCheck", "nokoriHpValue", "wazaDamage", "abicntMaxValue", "abicntMinValue",
                 "defensePokemonRndSelect", "wazaRndSelect", "wazaInitValue", "part", "wazaHit", "wazaAvgValue",
                 "wazaRisk", "wazaNokoriPpValue", "lastValueRevise"]
    p = os.path.join(d, "trainer_ai_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["ai"] + ai_fields + ["unnamed_1a_1d", "u16_1e", "zokusei_revise", "waza_type_revise"])
        for i, e in enumerate(table_rows(rel, 0x9bd14, 0x14a6b0, 0x28)):
            pairs = lambda o: "; ".join(f"type {e[o + 2 * k]}: {e[o + 2 * k + 1]}" for k in range(2))
            csvrow(f, [i] + list(e[:0x1A]) + [e[0x1A:0x1E].hex(), struct.unpack(">H", e[0x1E:0x20])[0], pairs(0x20), pairs(0x24)])
    note(p, "common", "common_rel section 4 +0x9bd14", "trainer ai", "CSV (decoded fields)")
    # story flag definitions (8 bytes; docs/engine/flags.md, verified): byte 0 bits 6-7 storage class, bits 0-5 width;
    # +1 initial-value index (7-word table lbl_80478ED4), +2 item-swap index, +3 event index, +4 u16 bit offset,
    # +6 s16 next flag in the story chain (head in the flag config lbl_80478F98 +4)
    fdefs = table_rows(rel, 0x111e04, 0x14a6c0, 8)
    ivb = world.common_table(rel, 0x80478ED4)
    init = [struct.unpack(">I", rel[ivb + 4 * k:ivb + 4 * k + 4])[0] for k in range(7)] if ivb else []
    cfg = world.common_table(rel, 0x80478F98)
    order = {}
    if cfg:
        cur = struct.unpack(">h", rel[cfg + 4:cfg + 6])[0]
        while 0 <= cur < len(fdefs) and cur not in order:
            order[cur] = len(order); cur = struct.unpack(">h", fdefs[cur][6:8])[0]
    p = os.path.join(d, "flags_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["flag", "storage_class", "save_section", "width_bits", "bit_offset", "initial_value_index",
                   "initial_value", "item_swap_index", "event_index", "next_flag", "story_chain_position"])
        # the bit offset is not on the disc: GSflagInitBitPos packs each class's flags in id order at startup (width
        # 0 counts as 1, over 32 as 32), and main.c points classes 1 / 2 / 3 at save sections 4 / 5 / 6 (verified)
        nxt = {1: 0, 2: 0, 3: 0}
        for i, e in enumerate(fdefs):
            cls, w = e[0] >> 6, e[0] & 0x3F
            w = 32 if w > 32 else (1 if w == 0 else w)
            pos = nxt.get(cls, "")
            if cls in nxt: nxt[cls] += w
            csvrow(f, [i, cls, {1: 4, 2: 5, 3: 6}.get(cls, ""), w, pos, e[1],
                       init[e[1]] if e[1] < len(init) else "", e[2], e[3], struct.unpack(">h", e[6:8])[0], order.get(i, "")])
    note(p, "common", "common_rel section 4 +0x111e04", "flags", "CSV (decoded fields)")
    # event table (heroDispatchEvent, docs/engine/flags.md: 0x1C bytes: u8 kind, u16 room id, u32 event id, u32 script
    # function, four u32 arguments); common_rel's prolog registers it (lbl_80478EC4, count lbl_80478EC0)
    eb, ec = world.common_table(rel, 0x80478EC4), world.common_table(rel, 0x80478EC0)
    if eb and ec:
        p = os.path.join(d, "events_decoded.csv")
        with open(p, "w", encoding="utf-8") as f:
            csvrow(f, ["index", "kind", "room_id", "room", "event_id", "script_id", "function", "arg0", "arg1", "arg2", "arg3"])
            for k in range(struct.unpack(">I", rel[ec:ec + 4])[0]):
                e = rel[eb + 0x1C * k:eb + 0x1C * (k + 1)]
                rid = struct.unpack(">H", e[2:4])[0]; w = struct.unpack(">6I", e[4:0x1C])
                # script function word: high u16 = id of a loaded script, low u16 = function (cms_ScriptCreateThread)
                csvrow(f, [k, e[0], rid, room_arch.get(rid, ""), w[0], f"{w[1] >> 16:#06x}", w[1] & 0xFFFF, *w[2:]])
        note(p, "common", "common_rel section 4 (lbl_80478EC4)", "events", "CSV (decoded fields)")
    # door table (lbl_80478ECC, count lbl_80478EC8, 0x18 bytes; floorEventCtrlDoor in src/game/floor_event.c and the
    # door loop in floor.c): s8 open / close / open-end / close-end animations, s8 two alternate animations (+5, +6),
    # u8 door type +7, u8 model part +8, s16 collision id +0x0A, u16 room id +0x0C, u16 open flag +0x0E, u16 close flag
    # +0x10, u32 room sub-resource id of the door model +0x14
    db, dc = world.common_table(rel, 0x80478ECC), world.common_table(rel, 0x80478EC8)
    if db and dc:
        p = os.path.join(d, "doors_decoded.csv")
        with open(p, "w", encoding="utf-8") as f:
            csvrow(f, ["door", "room_id", "room", "type", "model_part", "collision_id", "open_flag", "close_flag",
                       "model_resource", "anim_open", "anim_close", "anim_open_end", "anim_close_end", "anim_alt_a", "anim_alt_b"])
            for k in range(struct.unpack(">I", rel[dc:dc + 4])[0]):
                e = rel[db + 0x18 * k:db + 0x18 * (k + 1)]
                sb = lambda i: e[i] - 256 if e[i] > 127 else e[i]
                rid = struct.unpack(">H", e[0x0C:0x0E])[0]
                csvrow(f, [k, rid, room_arch.get(rid, ""), e[7], e[8], struct.unpack(">h", e[0x0A:0x0C])[0],
                           struct.unpack(">H", e[0x0E:0x10])[0], struct.unpack(">H", e[0x10:0x12])[0],
                           f"{struct.unpack('>I', e[0x14:0x18])[0]:#010x}", sb(0), sb(1), sb(2), sb(3), sb(5), sb(6)])
        note(p, "common", "common_rel section 4 (lbl_80478ECC)", "doors", "CSV (decoded fields)")
    # trainer AI scoring rules ("value add/sub", 0x14 bytes; fightTrainerAiValueAddsubDataBiosGet*): +0 s32 value,
    # +4 action name (koudou), +8 prefix name, +0xC name (message ids: Japanese developer labels), +0x10 u32 unnamed
    ab, ac = world.common_table(rel, 0x80478F2C), world.common_table(rel, 0x80478F28)
    if ab and ac:
        p = os.path.join(d, "trainer_ai_scores_decoded.csv")
        with open(p, "w", encoding="utf-8") as f:
            csvrow(f, ["rule", "value", "action", "decision", "condition", "u32_10"])
            for k in range(struct.unpack(">I", rel[ac:ac + 4])[0]):
                v, act, pre, nm, x = struct.unpack(">iIIII", rel[ab + 0x14 * k:ab + 0x14 * (k + 1)])
                csvrow(f, [k, v, txt(act) if act else "", txt(pre) if pre else "", txt(nm) if nm else "", x])
        note(p, "common", "common_rel section 4 (fight_trainer_ai_value_addsub_data)", "trainer ai scores", "CSV (decoded fields)")
    # menu sound groups (menuSeBiosGetPtr, 10 bytes = five u16 sound ids; fn_8005D798 picks one per menu event:
    # +0x04 event 1 cursor moved, +0x06 event 2 closed, +0x08 event 3 back, +0x00 event 4 (no caller in menu.c),
    # +0x02 event 5 opened; menuDaemon in src/game/menu.c). A menu's group = MenuData soundGroup & 7
    sb, sc = world.common_table(rel, 0x80478E94), world.common_table(rel, 0x80478E90)
    if sb and sc:
        p = os.path.join(d, "menu_sounds_decoded.csv")
        with open(p, "w", encoding="utf-8") as f:
            csvrow(f, ["group", "cursor_move", "close", "back", "event4", "open"])
            for k in range(struct.unpack(">I", rel[sc:sc + 4])[0]):
                e4, op, mv, cl, bk = struct.unpack(">5H", rel[sb + 10 * k:sb + 10 * (k + 1)])
                csvrow(f, [k, mv, cl, bk, e4, op])
        note(p, "common", "common_rel (menu_se)", "menu sounds", "CSV (decoded fields)")
    # menu faces (portraits; _menuFaceBiosGetPtr, 8 bytes): u8 flags (0x40 / 0xC0; bit 7's meaning open), 3 pad,
    # u32 FSYS entry id of the portrait texture; every id resolves to an entry of poke_face.fsys (verified, 2026-10-16)
    faces = {ident: nm for k_, nm, t_, ident, ld in fsys_entries(disc.file("poke_face.fsys"))}
    p = os.path.join(d, "menu_faces_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["face", "flags", "entry_id", "poke_face_entry"])
        for i, e in enumerate(table_rows(rel, 0x44, 0x14a65c, 8)):
            rid = struct.unpack(">I", e[4:8])[0]
            csvrow(f, [i, f"{e[0]:#04x}", f"{rid:#010x}", faces.get(rid, "") if rid else ""])
    note(p, "common", "common_rel section 4 +0x44", "menu faces", "CSV (decoded fields)")
    # shops: stock lists and clerk dialogue from the pocket_menu module (layout: colo.world.shops; the shop number is
    # the argument scripts pass to menuShopOpen)
    pm = [ld() for k_, n_, t_, ident, ld in fsys_entries(disc.file("pocket_menu.fsys")) if t_ == 14]
    if pm:
        sh = world.shops(pm[0])
        p = os.path.join(d, "shops_decoded.csv")
        with open(p, "w", encoding="utf-8") as f:
            csvrow(f, ["shop", "kind", "dialogue_set", "first_dialogue_text", "items (price; coupons at the exchanges)"])
            for s_ in sh["shops"]:
                # kinds 0/1 (marts, vending machine) take their dialogue from the 0x3C sets, the coupon exchanges
                # (kinds 2-4) from the 0x4C sets (likely: the text of each set matches its shop)
                dl = sh["dialogue_3c"] if s_["kind"] < 2 else sh["dialogue_4c"]
                ds = dl[s_["dialogue"]] if s_["dialogue"] < len(dl) else []
                first = next((txt(m_) for m_ in ds if m_), "")
                # price per item from the item table: coupon price (+0x08) at the coupon exchanges, else price (+0x06)
                po = 8 if s_["kind"] >= 2 else 6
                def price(i):
                    if i >= len(lookup) or lookup[i] >= nrec: return "?"
                    return struct.unpack(">H", rd(0x80363CE8 + lookup[i] * 0x28 + po, 2))[0]
                csvrow(f, [s_["index"], s_["kind"], s_["dialogue"], first,
                           "; ".join(f"{item_names.get(i, i)} ({price(i)})" for i in s_["items"])])
        note(p, "pocket_menu", "pocket_menu module section 3 (+0x1d8 shops, +0xe4 lists)", "shops", "CSV (decoded fields)")
    # Shadow Pokemon: each Shadow id with the species / level / trainers that carry it (trainer-Pokemon +0x03), plus
    # the definition record (lbl_80478F6C, 0x38 bytes, BattleRangeDef in battle_range_exact_801EEAD0.c; most fields are
    # story-flag ids whose meaning is open)
    owners = {}
    for ti, e in enumerate(tr_rows):
        first = struct.unpack(">H", e[4:6])[0]
        for k in range(first, first + 6):
            if k < len(tp) and tp[k][3]: owners.setdefault(tp[k][3], []).append((k, ti))
    p = os.path.join(d, "shadow_pokemon_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["shadow_id", "species", "levels", "trainers", "type", "species_02", "trainer_04", "trainer_06", "field_08", "flag0C", "flag0E", "flag12",
                   "flag14", "flag16", "flag1C", "action_list", "indexed_flags"])
        for sid, e in enumerate(table_rows(rel, 0x1439d8, 0x14a6f8, 0x38)):
            if sid == 0: continue
            w = struct.unpack(">16H", e[2:0x22]); own = owners.get(sid, [])
            sp = sorted({names.get(struct.unpack(">H", tp[k][0x0A:0x0C])[0], "") for k, ti in own})
            lv = sorted({tp[k][4] for k, ti in own})
            csvrow(f, [sid, "; ".join(sp), "; ".join(map(str, lv)), "; ".join(f"{ti}: {tname(ti)}" for k, ti in own), e[0],
                       w[0], w[1], w[2], w[3], w[5], w[6], w[8], w[9], w[10], w[13], w[14],
                       " ".join(str(v) for v in struct.unpack(">10H", e[0x24:0x38]) if v)])
    note(p, "common", "common_rel section 4 +0x1439d8", "shadow pokemon", "CSV (decoded fields)")
    # natures (0x28 bytes): +0x14 name message; +0x05..+0x09 one index per stat (Attack, Defense, Sp. Atk, Sp. Def,
    # Speed) into the nature-rate table ({110,100}, {100,100}, {90,100}, ...): 0 = +10 %, 1 = neutral, 2 = -10 %
    # (verified from data: all 25 match the standard Gen 3 chart); the other bytes are open
    p = os.path.join(d, "natures_decoded.csv")
    eff = {0: "+10%", 1: "", 2: "-10%"}
    with open(p, "w", encoding="utf-8") as f:
        # +0x00..+0x04: purification rate rows (PokemonSeikakuData reliveFightout / Walk / Call / Sodateya / Nadenade,
        # include/game/pokemon_data.h; the heart-gauge change = base x Kake / Waru of that row, pokemon-data.md)
        csvrow(f, ["nature", "name", "attack", "defense", "sp_atk", "sp_def", "speed", "purify_battle", "purify_walk",
                   "purify_call", "purify_daycare", "purify_scent", "bytes_0B_11", "bytes_18_25"])
        for n_, e in enumerate(table_rows(rel, 0x121614, 0x14a6d8, 0x28)):
            csvrow(f, [n_, txt(struct.unpack(">I", e[0x14:0x18])[0])] + [eff.get(b, b) for b in e[5:10]]
                   + [struct.unpack(">b", e[k:k + 1])[0] for k in range(5)] + [e[0x0B:0x12].hex(" "), e[0x18:0x26].hex(" ")])
    note(p, "common", "common_rel section 4 +0x121614", "natures", "CSV (decoded fields)")
    # smaller tables whose layouts the docs pin down (trainers.md, music.md, battle-arenas.md, game-data-tables.md)
    p = os.path.join(d, "trainer_classes_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["class", "name", "music_sound_id", "music_song", "word_00"])
        for c_, e in enumerate(table_rows(rel, 0x8f724, 0x14a684, 12)):
            sid = struct.unpack(">I", e[8:12])[0]
            csvrow(f, [c_, txt(struct.unpack(">I", e[4:8])[0]), sid, song(sid) or "", f"{struct.unpack('>I', e[0:4])[0]:#x}"])
    note(p, "common", "common_rel section 4 +0x8f724", "trainer classes", "CSV (decoded fields)")
    p = os.path.join(d, "battle_sides_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["side", "y_rotation", "trainer_status_menu_1", "trainer_status_menu_2", "pokemon_status_menu_1", "pokemon_status_menu_2"])
        for s_, e in enumerate(table_rows(rel, 0x8f910, 0x14a688, 20)):
            csvrow(f, [s_, struct.unpack(">H", e[0:2])[0]] + list(struct.unpack(">4I", e[4:20])))
    note(p, "common", "common_rel section 4 +0x8f910", "battle sides", "CSV (decoded fields)")
    songs_by_entry = {ident: nm for k_, nm, t_, ident, ld in fsys_entries(disc.file("bgm_archive.fsys"))}
    st_rows = table_rows(rel, 0x111b8c, 0x14a6bc, 8)
    p = os.path.join(d, "song_table_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["index", "bgm_entry_id", "song", "word_04"])
        for k_, e in enumerate(st_rows):
            eid = struct.unpack(">I", e[0:4])[0]
            csvrow(f, [k_, f"{eid:#010x}", songs_by_entry.get(eid, ""), f"{struct.unpack('>I', e[4:8])[0]:#x}"])
    note(p, "common", "common_rel section 4 +0x111b8c", "song table", "CSV (decoded fields)")
    p = os.path.join(d, "sound_ids_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["sound_id", "kind", "flags", "musyx_group", "reverb_preset", "song_table_index", "song", "musyx_id"])
        for sid_, e in enumerate(table_rows(rel, 0x13fe08, 0x14a6ec, 12)):
            fl_, grp, rev, wi = e[0], e[1], e[2], e[3]
            sng = songs_by_entry.get(struct.unpack(">I", st_rows[wi][0:4])[0], "") if not fl_ & 0x80 and wi < len(st_rows) else ""
            csvrow(f, [sid_, "resident effect" if fl_ & 0x80 else ("song" if sng else "none (placeholder)"), f"{fl_:#04x}", grp, rev, wi, sng,
                       struct.unpack(">H", e[4:6])[0]])
    note(p, "common", "common_rel section 4 +0x13fe08", "sound ids", "CSV (decoded fields)")
    p = os.path.join(d, "character_names_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["index", "name", "heard_flag_guess"])
        for c_ in world.character_names(rel): csvrow(f, [c_["index"], txt(c_["name_msg"]), c_["flag"]])
    note(p, "common", "common_rel (char_name)", "character names", "CSV (decoded fields)")
    p = os.path.join(d, "npc_info_decoded.csv")
    people = {}
    for arch_ in ("people_archive", "field_common"):
        for k_, nm, t_, ident, ld in fsys_entries(disc.file(arch_ + ".fsys")):
            if t_ == 2: people.setdefault(ident, nm)
    nbase = world.common_table(rel, 0x80478E7C)
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["npc_info", "model", "model_entry_id", "floats_10_28"])
        for c_ in world.npc_info(rel):
            r_ = rel[nbase + c_["index"] * 0x2C:nbase + c_["index"] * 0x2C + 0x2C]
            csvrow(f, [c_["index"], people.get(c_["model"], ""), f"{c_['model']:#010x}",
                       " ".join(f"{v:g}" for v in struct.unpack(">7f", r_[0x10:0x2C]))])
    note(p, "common", "common_rel (peopleInfoData)", "npc info", "CSV (decoded fields)")
    # rooms and every NPC placed in them (colo/world.py: floor table, room modules' character tables)
    p = os.path.join(d, "rooms_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["floor_index", "room_id", "archive", "area", "class", "map_entry_id"])
        for r_ in fl:
            arch = r_["archive"] or ""; m_ = re.match(r"([A-Z]\d)_", arch)
            csvrow(f, [r_["index"], r_["room_id"], arch, AREAS.get(m_.group(1), "").replace("_", " ") if m_ else "", f"{r_['cls']:#04x}", f"{r_['map_res']:#x}"])
    note(p, "common", "common_rel section 4 +0x8bcf4", "floors", "CSV (decoded fields)")
    p = os.path.join(d, "npc_placements_decoded.csv")
    with open(p, "w", encoding="utf-8") as f:
        csvrow(f, ["room", "slot", "model", "name", "x", "y", "z", "talk_script", "move_script", "npc_info", "flags"])
        for arch, pl in sorted(placements(disc).items()):
            for c in pl["chars"]:
                csvrow(f, [arch, c["index"], c.get("model", ""), c.get("who", ""), *[f"{v:.2f}" for v in c["pos"]],
                           f"{c['talk_script']:#010x}", f"{c['move_script']:#010x}", c["info"], f"{c['flags']:#04x}"])
    note(p, "common", "room modules (type 14) + common_rel NPC info / names", "npc placements", "CSV (decoded fields)")
    print(f"{len(TABLES)} raw tables + species.json, moves_decoded.csv, reverb_presets_decoded.csv, items_decoded.csv, "
          f"trainers_decoded.csv, trainer_pokemon_decoded.csv, arenas_decoded.csv, battles_decoded.csv, treasure_decoded.csv, shops_decoded.csv, battle_kinds_decoded.csv, menu_faces_decoded.csv, trainer_ai_decoded.csv, flags_decoded.csv, events_decoded.csv, doors_decoded.csv, trainer_ai_scores_decoded.csv, menu_sounds_decoded.csv, shadow_pokemon_decoded.csv, natures_decoded.csv, trainer_classes, battle_sides, song_table, sound_ids, character_names, npc_info (_decoded.csv), rooms_decoded.csv, npc_placements_decoded.csv -> {d}")


def wav(path, rate, samples):
    data = struct.pack(f"<{len(samples)}h", *samples)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " +
                struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) + b"data" + struct.pack("<I", len(data)) + data)

def cmd_audio(disc, out):
    """MusyX sample banks: snd_<bank>_sdir (common.fsys) describes the samples in sound/snd_<bank>.samp."""
    base = os.path.join(out, "audio")
    readme(base, "Every sound sample (music instruments, sound effects, Pokémon cries) as WAV, one folder per MusyX sample bank.",
           "WAV, 16-bit mono PCM at the sample's own rate, decoded from GameCube DSP-ADPCM. Files are named "
           "<sample id>_<rate>Hz[_loop<start>-<end>].wav. These are the raw instrument / effect samples: songs are "
           "sequences (bgm_archive) that play them, so music is not rendered as whole songs.",
           evidence=[("Sample directory layout", "SdirDataT in src/musyx/runtime/synthdata.c: id, offset, header {info = base note << 24 | rate, "
                      "length, loop offset, loop length, ADPCM info offset}", "verified (code + data)"),
                     ("DSP-ADPCM", "standard GameCube format: 8-byte frames, 8 coefficient pairs", "verified (decodes to clean audio; listen to confirm)"),
                     ("Bank roles", "names: music, music_atmos (ambience), se (effects), se_battle, se_motion, se_nakigoe / nakigoe2 (cries)", "guess (names)")],
           docs=["docs/engine/extractor.md", "docs/fable5_audit_pass1_musyx_discovery.md"])
    com = {name: load for k, name, t, ident, load in fsys_entries(disc.file("common.fsys"))}
    n = 0
    for name in sorted(com):
        m = re.match(r"snd_(.+)_sdir$", name)
        if not m or f"sound/snd_{m.group(1)}.samp" not in disc.files: continue
        sdir = com[name](); samp = disc.file(f"sound/snd_{m.group(1)}.samp")
        folder = mkdir(os.path.join(base, m.group(1))); wavs = []
        readme(folder, f"Samples of the MusyX bank '{m.group(1)}'.", "WAV 16-bit mono", f"common.fsys entry {name} + sound/snd_{m.group(1)}.samp")
        for smp in sdir_samples(sdir, samp):
            sid, off, rate, nsamp = smp["id"], smp["offset"], smp["rate"], smp["samples"]; loop_off, loop_len = smp["loop"]
            looped = loop_len and loop_off != 0xFFFFFFFF and loop_off + loop_len <= nsamp  # 0xFFFFFFFF = no loop
            label = f"{sid:04X}_{rate}Hz" + (f"_loop{loop_off}-{loop_off + loop_len}" if looped else "")
            p = os.path.join(folder, label + ".wav"); wav(p, rate, smp["pcm"]); n += 1; wavs.append(p)
            SOURCES.setdefault(os.path.abspath(folder), {}).setdefault(f"sound/snd_{m.group(1)}.samp", set()).add(f"offset {off:#x}")
            MANIFEST.append((os.path.abspath(p), f"common.fsys:{name} + sound/snd_{m.group(1)}.samp", f"sample {sid:#06x} at {off:#x}",
                             f"sample {sid:#06x}", "WAV (from DSP-ADPCM)", f"{nsamp} samples at {rate} Hz", ""))
        keep_original(folder, "common", name, name, 0, sdir, wavs, "MusyX sample directory (SdirDataT entries + ADPCM info)", ext="sdir",
                      info={"sdir": True, "names": [(f"sample directory of the '{m.group(1)}' bank (music / effects / cries by name)", G)]})
        keep_original(folder, f"sound/snd_{m.group(1)}.samp", "(disc file)", f"snd_{m.group(1)}", 0, samp, wavs,
                      "MusyX sample data (DSP-ADPCM)", ext="samp", info={"sdir": True})
    mdir = mkdir(os.path.join(base, "musyx_data"))  # readable dump of each bank's project + pool (colo/musyx.py)
    for name in sorted(com):
        m = re.match(r"snd_(.+)_proj$", name)
        if not m or f"snd_{m.group(1)}_pool" not in com: continue
        t = os.path.join(mdir, f"{m.group(1)}_bank.txt")
        open(t, "w", encoding="utf-8").write(bank_listing(com[name](), com[f"snd_{m.group(1)}_pool"](), name=m.group(1)) + chr(10))
        note(t, "common", f"{name} + snd_{m.group(1)}_pool", name, "text: MusyX bank (groups, programs, MIDI setups, layers, keymaps, macros)")
    print(f"{n} samples -> {base}")

def unicode_block(c):
    for lo, hi, n in ((0x20, 0x7F, "Basic Latin (ASCII)"), (0xA0, 0x100, "Latin-1 Supplement"), (0x100, 0x180, "Latin Extended-A"),
                      (0x2000, 0x2070, "General Punctuation"), (0x2100, 0x2200, "Letterlike / arrows"), (0x2190, 0x2200, "Arrows"),
                      (0x2460, 0x2700, "Symbols and shapes"), (0x3000, 0x3040, "CJK punctuation"), (0x3040, 0x30A0, "Hiragana"),
                      (0x30A0, 0x3100, "Katakana"), (0x4E00, 0xA000, "Kanji (CJK ideographs)"), (0xE000, 0xF900, "Private use (game symbols)"),
                      (0xFF00, 0xFFF0, "Full-width forms")):
        if lo <= c < hi: return n
    return "Other"

def write_font(folder, label, font, src_note):
    """One font: PNG sheet + character map CSV; returns the list of written files."""
    W, H, px = atlas(font); sheet = os.path.join(folder, f"{label}_font{font['id']}_{font['cell_w']}px.png"); png(sheet, W, H, px)
    cmap = sheet[:-4] + "_charmap.csv"
    with open(cmap, "w", encoding="utf-8") as f:
        f.write("index,char,codepoint,block,width_advance,height,y_offset,sheet_column,sheet_row\n")
        for i, g in enumerate(font["glyphs"]):
            ch = chr(g["char"]) if g["char"] >= 0x20 and not 0xD800 <= g["char"] < 0xE000 else ""
            ch = '""' if ch == '"' else ch
            f.write(f'{i},"{ch}",U+{g["char"]:04X},{unicode_block(g["char"])},{g["width"]},{g["height"]},{g["y_offset"]},{i % 32},{i // 32}\n')
    from colo.font import metrics
    met = sheet[:-4] + "_metrics.json"
    m = dict(metrics(font), source=f"{src_note[0]}: {src_note[1]}",
             glyphs=[{"char": chr(g["char"]) if g["char"] >= 0x20 and not 0xD800 <= g["char"] < 0xE000 else "",
                      "codepoint": f"U+{g['char']:04X}", "advance": g["width"], "width": g["width"], "height": g["height"],
                      "y_offset": g["y_offset"], "sheet_column": i % 32, "sheet_row": i // 32} for i, g in enumerate(font["glyphs"])])
    json.dump(m, open(met, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    for path, fmt in ((sheet, "PNG glyph sheet (table order, 32 per row; cell outlines, orange tick = advance width)"),
                      (cmap, "CSV character map"), (met, "JSON metrics (line height, spacing, per-glyph metrics)")):
        note(path, *src_note, fmt)
    return [sheet, cmap, met]

def write_special_symbols(disc, base, rel):
    """fonts/special_symbols/: each code point the game draws as its own symbol (colo/font.py SPECIAL_SYMBOLS;
    docs/engine/fonts.md "Special symbols") as an 8x image per font size it appears in (U+XXXX_<tag>_<cell>px_8x.png),
    plus overview.png with every symbol labelled in the game's own main font."""
    from colo.font import SPECIAL_SYMBOLS, glyph_image, label_sheet
    wanted = {cp for cp, _, _ in SPECIAL_SYMBOLS}; found = {}  # (cp, cell) -> (glyph, cell, font label)
    main = parse_fonts(rel[SEC4 + 0x2d2c:])[0]
    sources = [(f"common_rel resource {i}", rel[SEC4 + off:]) for i, off in ((0, 0x2d2c), (2, 0x45a94), (3, 0x494e4))]
    for arch, arc in archives(disc):
        for k, name, t, ident, load in fsys_entries(arc):
            if t == 6: sources.append((f"{arch}/{name}", load()))
    for label, data in sources:
        for f in parse_fonts(data):
            for g in f["glyphs"]:
                if g["char"] in wanted: found.setdefault((g["char"], f["cell_h"]), (g, f["cell_h"], label))
    d = mkdir(os.path.join(base, "special_symbols")); entries = []
    readme(d, "The code points the game draws as its own symbols (Pokemon dollar, speech bubble, e-Reader marks, ...): one 8x "
              "image per symbol and font size, and overview.png with all of them labelled.",
           "PNG, 8x enlarged with hard pixel edges, dark glyph on white (the game draws it in the text colour); file name "
           "U+XXXX_<what it is>_<cell>px_8x.png", "the fonts in common_rel and the room / menu archives",
           evidence=[("Which code points", "a scan of all 221 fonts against reference fonts, every candidate checked by eye", "verified (glyph shapes)")],
           docs=["docs/engine/fonts.md"])
    for cp, tag, what in SPECIAL_SYMBOLS:
        for cell in (22, 21, 18):
            if (cp, cell) not in found: continue
            g, ch, label = found[(cp, cell)]
            w, h, px = glyph_image(g, ch)
            p = os.path.join(d, f"U+{cp:04X}_{tag}_{cell}px_8x.png"); png(p, w, h, px)
            note(p, label.split("/")[0], label, f"U+{cp:04X}", f"PNG 8x of the special symbol: {what}")
            entries.append((g, ch, f"U+{cp:04X} {cell}px"))
    if entries:
        W, H, px = label_sheet(entries, main)
        p = os.path.join(d, "overview.png"); png(p, W, H, px); note(p, "", "", "overview", "PNG overview of all special symbols")


def write_font_index(disc, base, rel):
    """fonts/FONTS.md: every font file on the disc, where the game registers it, its fonts' metrics, and how often the
    text switches to each font id (control code 0x07). docs/engine/fonts.md explains the rules."""
    from colo.font import metrics
    L = ["# Fonts on the USA disc (generated)", ""] + BANNER + [
        "How the format and the layout rules work: `docs/engine/fonts.md`. Per-font sheets, character maps and metrics are in",
        "`main/` and `rooms/<archive>/`.", "", "## Main fonts (registered at startup by main.c)", "",
        "| Source | Font id | Cell | Glyphs | Line height | Space advance |", "|---|---|---|---|---|---|"]
    for name, off in [("common_rel resource 0", 0x2d2c), ("common_rel resource 1", 0x45a8c), ("common_rel resource 2", 0x45a94),
                      ("common_rel resource 3", 0x494e4)]:
        fs = parse_fonts(rel[SEC4 + off:])
        if not fs: L.append(f"| {name} | (empty: the file is only the end marker 0xFFFF) | | | | |")
        for f in fs:
            m = metrics(f)
            L.append(f"| {name} | {f['id']} | {f['cell_w']}x{f['cell_h']} | {len(f['glyphs'])} | {m['line_height']} | {m['space_advance']} |")
    L += ["", "main.dol also carries byte-identical copies of resources 0, 2 and 3 (`lbl_8027A500`, `lbl_802BD260`,",
          "`lbl_802C0CB0`); main.c registers both copies.", "", "## Fonts in room and menu archives (registered when the room loads)", "",
          "| Archive | Entry | Fonts (id: cell, glyphs) |", "|---|---|---|"]
    seen = set()
    for arch, arc in archives(disc):
        for k, name, t, ident, load in fsys_entries(arc):
            if t != 6: continue
            fs = parse_fonts(load())
            L.append(f"| {arch} | {name}{' (same entry as an earlier archive)' if ident in seen else ''} | "
                     + "; ".join(f"{f['id']}: {f['cell_w']} px, {len(f['glyphs'])}" for f in fs) + " |")
            seen.add(ident)
    uses = {}
    for k_, v in all_text(disc).items():
        for x in re.findall(r"<07:([0-9A-Fa-f]+)>", v): uses[int(x, 16)] = uses.get(int(x, 16), 0) + 1
    L += ["", "## Font switches in the text (control code 0x07 + font id)", "", "| Font id | Messages switching to it |", "|---|---|"]
    L += [f"| {fid} | {n} |" for fid, n in sorted(uses.items())]
    p = os.path.join(base, "FONTS.md"); open(p, "w", encoding="utf-8").write("\n".join(L) + "\n")
    note(p, "", "", "FONTS", "Markdown index")


def cmd_fonts(disc, out):
    base = os.path.join(out, "fonts")
    readme(base, "Every font: the main fonts (inside common_rel) and each room's extra-glyph font, as PNG sheets with character maps; COVERAGE.md lists which characters exist (for language support).",
           "PNG glyph sheet (light glyphs on dark, 32 per row, in table order; cell outlines, orange tick = advance width) + *_charmap.csv (character, Unicode code point, block, width = advance, height, vertical offset, sheet position) + *_metrics.json (cell size, line gap and line height, space and missing-glyph advance, per-glyph metrics); FONTS.md lists every font file, where it is registered and how often the text uses each font id",
           evidence=[("Font layout", "glyph lookup fn_800FD348 and copy fn_800FD69C (gs_msg.c): stride ceil(width/2), data at header + 8 + offset, s8 y offset", "verified (code)"),
                     ("Characters are Unicode", "glyph codes match the UTF-16 text (A = U+0041, kana U+30xx)", "verified (data)")],
           docs=["docs/engine/fonts.md", "docs/engine/messages.md"])
    rel = common_rel(disc); main = os.path.join(base, "main"); all_chars = {}
    readme(main, "The game's main fonts, stored inside common_rel's resource table (tableResBiosGetResPtr entries 0, 2, 3).",
           "PNG + character map per font", "common.fsys -> common_rel (section 4 offsets 0x2d2c, 0x45a94, 0x494e4)")
    for name, off in REL_FONTS:
        for f in parse_fonts(rel[SEC4 + off:]):
            files = write_font(mkdir(main), name, f, ("common", f"common_rel section 4 +{off:#x}", name))
            for g in f["glyphs"]: all_chars.setdefault(g["char"], set()).add(f"{name} font {f['id']}")
    rooms = os.path.join(base, "rooms"); seen = set(); n = 0
    readme(rooms, "Each room's / menu's own font: only the extra glyphs (mostly kanji) its text needs, one folder per archive.",
           "PNG + character map per font; originals in each folder's original/")
    for arch, arc in archives(disc):
        for k, name, t, ident, load in fsys_entries(arc):
            if t != 6 or ident in seen: continue
            seen.add(ident); data = load(); folder = mkdir(os.path.join(rooms, safe(arch))); files = []
            detail(folder, archive_details(disc, arch))
            for f in parse_fonts(data):
                files += write_font(folder, safe(name), f, (arch, f"{k} ({name})", name)); n += 1
                for g in f["glyphs"]: all_chars.setdefault(g["char"], set()).add(f"{arch}/{name}")
            keep_original(folder, arch, f"{k} ({name})", name, 6, data, files,
                          info={"names": [(f"font entry `{name}` of {arch}.fsys", V)]})
    write_font_index(disc, base, rel)
    write_special_symbols(disc, base, rel)
    cov = os.path.join(base, "COVERAGE.md"); blocks = {}
    for c in sorted(all_chars): blocks.setdefault(unicode_block(c), []).append(c)
    lines = ["# Font character coverage (USA disc)", "", "Every character any font on the disc can draw, by Unicode block. "
             "Text outside this set cannot be shown without adding glyphs (matters for translations).", "",
             "| Block | Characters | Count |", "|---|---|---|"]
    for b, cs in blocks.items():
        shown = "".join(chr(c) for c in cs if c >= 0x20 and not 0xE000 <= c < 0xF900)
        lines.append(f"| {b} | {shown[:300].replace('|', '&#124;')}{' ...' if len(shown) > 300 else ''} | {len(cs)} |")
    lines += ["", f"Total distinct characters: {len(all_chars)}.", "",
              "Main-font characters only (what any text can use anywhere): see main/*_charmap.csv."]
    open(cov, "w", encoding="utf-8").write("\n".join(lines) + "\n"); note(cov, "", "", "COVERAGE", "Markdown summary")
    print(f"{n} room fonts + main fonts -> {base} ({len(all_chars)} distinct characters)")

def dol_reader(disc):
    dol = disc.dol()
    o = struct.unpack(">18I", dol[0:72]); a = struct.unpack(">18I", dol[72:144]); z = struct.unpack(">18I", dol[144:216])
    def rd(x, n):
        for O, A, S in zip(o, a, z):
            if A <= x < A + S: return dol[O + x - A:O + x - A + n]
        return b"\0" * n
    return rd

def symbol_names():
    out = {}
    for l in open(os.path.join(REPO, "config", "GC6E01", "symbols.txt"), encoding="utf-8"):
        m = re.match(r"(\S+) = \.text:0x([0-9A-F]+);", l)
        if m: out[int(m.group(2), 16)] = m.group(1)
    return out

def music_lookup(disc, rel):
    """sound id -> song file name (sound table +0x03 = song-table index; song table first word = bgm entry id)."""
    songs = {ident: name for k, name, t, ident, load in fsys_entries(disc.file("bgm_archive.fsys"))}
    st = table_rows(rel, 0x111b8c, 0x14a6bc, 8); snd = table_rows(rel, 0x13fe08, 0x14a6ec, 12)
    def song(sid):
        try: sid = int(sid)
        except (TypeError, ValueError): return None
        if not 0 < sid < len(snd): return None
        wi = snd[sid][3]
        return songs.get(struct.unpack(">I", st[wi][:4])[0]) if wi < len(st) else None
    return song

_SCRIPT = {}
def script_ctx(disc):
    """(natives, annotate) for colo/script.listing, cached."""
    if _SCRIPT: return _SCRIPT["natives"], _SCRIPT["annotate"]
    rd = dol_reader(disc); syms = symbol_names()
    natives = {i: syms.get(f, f"fn_{f:08X}") for i, (f, t) in native_table(rd).items()}
    rel = common_rel(disc); msgs = all_text(disc); song = music_lookup(disc, rel)
    rooms = {}
    for e in table_rows(rel, 0x8bcf4, 0x14a670, 0x4C):
        rooms[struct.unpack(">I", e[0x0C:0x10])[0]] = struct.unpack(">I", e[4:8])[0]
    toc = gsfsys_toc(disc)
    def annotate(name, args):
        notes = []
        low = name.lower()
        if "backgroundmusic" in low or "bgm" in low:
            sg = song(args[0]) if args else None
            if sg: notes.append(f"music: {sg}")
        if any(k in low for k in ("msg", "talk", "message", "yesno", "dialog")):
            for x in args:
                try: v = int(x)
                except (TypeError, ValueError): continue
                if v > 0xFF and v in msgs: notes.append(f'msg {v:#x}: "{clean(msgs[v])[:90]}"')
        if name == "std078_flagSetFalse": notes.append("SETS the flag (writes 1): symbol name is swapped, docs/engine/proposed-names.md")
        if name == "std079_flagSetTrue": notes.append("CLEARS the flag (writes 0): symbol name is swapped")
        if name in ("floorLink", "floorLinkWithSE", "std098_mapWarpToMenuRoom"):
            for x in args[:1]:
                try: v = int(x)
                except (TypeError, ValueError): continue
                if v in rooms: notes.append(f"room {v:#x} = {toc.get(rooms[v], '?')}")
        return "; ".join(notes)
    _SCRIPT.update(natives=natives, annotate=annotate)
    return natives, annotate

def script_songs(lines):
    return sorted({ln.split("music: ")[1].split(";")[0].strip() for ln in lines if "music: " in ln})

def room_script_songs(disc, arc):
    natives, annotate = script_ctx(disc); out = set()
    for k, name, t, ident, load in fsys_entries(arc):
        if t == 7:
            try: out |= set(script_songs(listing(load(), natives, annotate)[1]))
            except Exception: pass
    return sorted(out)

_PEOPLE = set()
def object_details(disc, name):
    """An object / NPC model in a room: where else the same model is (shared NPC archives, trainer battle models)."""
    if not _PEOPLE:
        for arch in ("people_archive", "field_common"):
            _PEOPLE.update(n for k, n, t, i, l in fsys_entries(disc.file(arch + ".fsys")) if t == 2)
    items = []
    if name in _PEOPLE: items.append((f"the same model is in the shared NPC archives: extracted/npcs/{safe(name)}/ (people_archive / field_common)", V))
    m = re.match(r"(.+)_b(\d+)$", name)
    if m and f"pkx_{m.group(1)}_a1.fsys" in disc.files:
        items.append((f"battle model of the same character: pkx_{m.group(1)}_a1 (extracted/trainers/); _b = overworld, _a = battle", G))
    room = getattr(object_details, "room", None)
    pl = placements(disc).get(room) if room else None
    if pl:
        us = [c for c in pl["chars"] if c["model"] == name]
        if us: items.append((f"placed in this room as: " + "; ".join((c["who"] or f"character {c['index']}") + f" at ({c['pos'][0]:.0f}, {c['pos'][1]:.0f}, {c['pos'][2]:.0f})" for c in us), V))
    if name.lower().startswith("common_news") or name.startswith("CommonNews"):
        items.append(("a news / notice board object shared by many rooms (name)", G))
    return items

def cmd_scripts(disc, out):
    base = os.path.join(out, "scripts")
    readme(base, "Every event script, disassembled into a readable listing: one file per script, grouped like maps/ (area / room), menus in menus/.",
           "<archive>__<entry>.txt: a header (room, area, songs it starts, functions) and the listing; native calls are rebuilt as name(args) with message text, song and room names as comments. Originals (.scd) in original/.",
           evidence=[("Opcode table and operand sizes", "the 38 cms_opNN handlers (gs_thread.c, dispatch table lbl_803155D0); jump targets land on instruction boundaries in every script", "verified (code + data)"),
                     ("Native names", "native table lbl_802E1CF0 (main.dol) -> symbols.txt names", "verified"),
                     ("Expression rebuilding", "from the stack behaviour of each handler; some opcode meanings (ops 13-36) come from their symbol names", "partly guess")],
           docs=["docs/engine/script-vm.md", "docs/engine/music.md"])
    natives, annotate = script_ctx(disc)
    n = 0
    for arch, arc in archives(disc):
        for k, name, t, ident, load in fsys_entries(arc):
            if t != 7: continue
            data = load()
            m = re.match(r"([A-Z]\d)_", arch)
            group = f"{m.group(1)}_{AREAS.get(m.group(1), 'Unknown')}" if m else "menus"
            folder = mkdir(os.path.join(base, group))
            hdr, lines = listing(data, natives, annotate)
            songs = script_songs(lines)
            path = os.path.join(folder, f"{safe(arch)}__{safe(name)}.txt")
            with open(path, "w", encoding="utf-8") as f:
                f.write(f"; script `{name}` of {arch}.fsys (entry {k})\n")
                f.write(f"; room / scene: {arch}" + (f" ({group.replace('_', ' ', 1)})" if m else " (menu / screen)") + "\n")
                f.write(f"; songs this script starts: {', '.join(songs) if songs else '(none)'}\n")
                f.write(f"; header words {[hex(x) for x in hdr['words']]}, {len(hdr['functions'])} functions\n")
                f.write("; legend: L = local variable, G = global, name(args) = native call (stdNNN = native NNN),\n")
                f.write(";         funcN(...) = call to function N of this script; 'if !(x) goto' = jump_false\n")
                f.write("\n".join(lines) + "\n")
            note(path, arch, f"{k} ({name})", name, "script listing (text)")
            keep_original(folder, arch, f"{k} ({name})", name, 7, data, path,
                          info={"names": [(f"event script of {arch}", V)],
                                "format": [(f"{len(hdr['functions'])} functions; header words {[hex(x) for x in hdr['words']]}", V)]})
            n += 1
    # the shared common script (id 0x0596): not an archive entry but embedded in common_rel, section 4 +0x89CFC (found
    #, docs/engine/common-script.md); every hero-event record and many room scripts call into it
    rel = common_rel(disc); cs = 0x184C + 0x89CFC
    if rel[cs:cs + 2] == b"\x05\x96":
        from colo.script import disassemble, parse_header
        h = parse_header(rel[cs:cs + 0x200])
        end = len(rel) - cs
        for at, nm_, kd, a in disassemble(rel[cs:], start=max(h["functions"])):
            if nm_.startswith("bad_"): end = at; break
        data = rel[cs:cs + end]
        hdr, lines = listing(data, natives, annotate)
        folder = mkdir(os.path.join(base, "common"))
        path = os.path.join(folder, "common_rel__0596.txt")
        with open(path, "w", encoding="utf-8") as f:
            f.write("; the shared common script (id 0x0596), embedded in common_rel section 4 at +0x89CFC\n")
            f.write(f"; header words {[hex(x) for x in hdr['words']]}, {len(hdr['functions'])} functions, {end:#x} bytes\n")
            f.write("; function roles: docs/engine/common-script.md (e.g. 4 = exit, 5 = door, 8 / 9 = fade in / out, 11 = sign,\n")
            f.write(";   13 = PC box, 19 = TV news, 21 = story mail scheduler)\n")
            f.write("\n".join(lines) + "\n")
        note(path, "common", "common_rel section 4 +0x89cfc", "common script", "script listing (text)")
        readme(folder, "The shared common script (id 0x0596): the functions every room and the hero-event table call (exits, doors, fades, signs, PC, TV, mail).",
               "one readable listing (same format as the room scripts)", "common.fsys -> common_rel, section 4 +0x89CFC",
               evidence=[("Location and id", "header id 0x0596 at that offset; script function words 0x0596xxxx call it (cms_ScriptCreateThread matches the id)", "verified (code + data)")],
               docs=["docs/engine/common-script.md", "docs/engine/script-vm.md"])
        n += 1
    print(f"{n} scripts -> {base}")
    from colo import flagxref  # story flag cross-reference over the listings just written (docs/engine/flags.md)
    xref, dynamic = flagxref.scan(base)
    with open(os.path.join(base, "FLAGS.md"), "w", encoding="utf-8") as f:
        f.write(flagxref.text(xref, dynamic) + "\n")
    print(f"{len(xref)} story flags cross-referenced -> {os.path.join(base, 'FLAGS.md')}")

LIKELY = {  # resource type -> what it most likely is (docs/engine/fsys-catalog.md, floor-resources.md)
    0: "generic data (MusyX project/pool/sample-directory files, e-Reader card data, keys)",
    1: "map model (HSD)", 2: "object / character model (HSD)", 3: "collision", 4: "BGM song sequence (MusyX)",
    6: "font", 7: "script bytecode (docs/engine/script-vm.md)", 10: "particles", 12: "camera animation",
    14: "REL module (code + data, linked at load)", 17: "GFL (unknown)", 18: "particles (not linked)", 19: "REL module (code)"}

MOVIES = [  # disc file, readable name, what it shows / story point (docs/engine/movies.md), code that plays it
    ("movie/gs_logo.thp", "Boot_Genius_Sonority_logo", "Genius Sonority logo at boot", "not found yet (boot sequence)"),
    ("movie/tpc.thp", "Boot_The_Pokemon_Company_logo", "The Pokémon Company logo at boot", "not found yet (boot sequence)"),
    ("movie/openingdemo.thp", "Opening_Wes_steals_the_Snag_Machine",
     "the opening cinematic of a new game: Wes blows up the Team Snagem hideout and escapes with the Snag Machine",
     "movieOpeningStart; afterwards floorRequestChange(0x384) and a fade script"),
    ("movie/autodemo01.thp", "Title_screen_attract_demo", "the attract demo shown when the title screen is left idle",
     "movieStartAttract (title screen; BGM 0x494)"),
    ("movie/staffroll.thp", "Credits_staff_roll", "the staff credits after the ending", "fn_80035F64 / fn_800361C0"),
]

def ffmpeg_path():
    import shutil
    for c in (shutil.which("ffmpeg"), os.path.join("C:/", "Windows", "ffmpeg"), os.path.join("C:/", "Windows", "ffmpeg.exe")):
        if c and os.path.exists(c): return c
    return None

def cmd_movies(disc, out):
    """The five THP movies: originals copied byte for byte, plus an MP4 (H.264 + AAC) of each made with FFmpeg."""
    import subprocess
    base = os.path.join(out, "movies")
    readme(base, "The game's five pre-rendered videos (THP) as MP4, named by what they show. Every story cutscene "
           "is rendered in-engine; only these five are videos.",
           "MP4: H.264 video (CRF 18, yuv420p) + AAC audio, converted with FFmpeg from the THP originals in original/. "
           "Sound effects in the videos are NOT in the THP audio track: the game fires them on frame numbers from a cue "
           "table (docs/engine/movies.md), so the MP4s have the THP audio track only. gs_logo, autodemo01 and staffroll have "
           "no audio track at all (THP header: one video component, audio max samples 0): the game plays their music "
           "separately through MusyX (attract demo: BGM 0x494), so those three MP4s are silent.",
           "movie/*.thp on the disc (plain files, not in an FSYS archive)",
           evidence=[(f"`{name}.mp4`: {what}; played by {code}", "docs/engine/movies.md", "verified (code)" if "not found" not in code else "guess")
                     for path, name, what, code in MOVIES]
                    + [("THP container (Motion-JPEG frames + interleaved ADPCM audio), header magic `THP` + zero byte", "header bytes; FFmpeg's THP demuxer reads them", "verified (data)")],
           docs=["docs/engine/movies.md", "docs/tools.md"])
    ff = ffmpeg_path()
    if not ff: print("movies: FFmpeg not found; originals only")
    for path, name, what, code in MOVIES:
        if path not in disc.files: continue
        data = disc.file(path)
        mp4 = os.path.join(base, f"{name}.mp4")
        orig = keep_original(base, path, 0, os.path.splitext(os.path.basename(path))[0], "thp", data, mp4, ext="thp",
                             info=dict(names=[(f"shows {what}", V), (f"converted copy: {name}.mp4", V)],
                                       code=[(code, V if "not found" not in code else G)],
                                       format=[(f"{struct.unpack('>I', data[0x14:0x18])[0]} frames at {struct.unpack('>f', data[0x10:0x14])[0]:.2f} fps", V)],
                                       port=[("vanilla mode plays the original; an in-engine remake is an optional mod (design-notes.md)", V)]))
        if ff and (not os.path.exists(mp4) or os.path.getsize(mp4) == 0):
            r = subprocess.run([ff, "-hide_banner", "-loglevel", "error", "-y", "-i", orig, "-c:v", "libx264", "-crf", "18",
                                "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", mp4], capture_output=True, text=True)
            if r.returncode: print("movies: ffmpeg failed for", path, r.stderr[-300:])
        if os.path.exists(mp4): note(mp4, path, 0, os.path.basename(path), "MP4 (H.264 + AAC) converted with FFmpeg", str(os.path.getsize(mp4)))

def cmd_songs(disc, out):
    """MusyX songs (bgm_archive type 4) -> standard MIDI + an instrument map per song (colo/song.py, colo/musyx.py)."""
    from colo import song as songlib
    base = mkdir(os.path.join(out, "audio", "songs"))
    rel = common_rel(disc)
    com = {name: load for k, name, t, ident, load in fsys_entries(disc.file("common.fsys"))}
    banks = {}  # MusyX group id -> (bank name, group, pool)
    for bank in ("music", "music_atmos"):
        pool = parse_pool(com[f"snd_{bank}_pool"]())
        for g in parse_project(com[f"snd_{bank}_proj"]()):
            banks[g["id"]] = (bank, g, pool)
    entry_names = {ident: name for k, name, t, ident, load in fsys_entries(disc.file("bgm_archive.fsys"))}
    st = table_rows(rel, 0x111b8c, 0x14a6bc, 8); snd = table_rows(rel, 0x13fe08, 0x14a6ec, 12)
    starts = {}  # song name -> [(sound id, group, song id)]: how the game starts it (sound table, music.md)
    for sid, e in enumerate(snd):
        if sid == 0 or e[0] & 0x80 or e[3] >= len(st): continue
        nm = entry_names.get(struct.unpack(">I", st[e[3]][:4])[0])
        if nm: starts.setdefault(nm, []).append((sid, e[1], struct.unpack(">H", e[4:6])[0]))
    wavs = {}  # sample id -> WAV written by stage `audio` (linked when present)
    for bank in ("music", "music_atmos"):
        d = os.path.join(out, "audio", bank)
        if os.path.isdir(d):
            for f in sorted(os.listdir(d), key=len, reverse=True):  # shortest name wins: an older run may have left a
                if f.endswith(".wav"): wavs[int(f[:4], 16)] = f"../{bank}/{f}"  # misnamed duplicate (0245_..._loop4294967295-...)
    readme(base, "Every song (BGM): a standard MIDI file of its notes and a text file with its tempo, tracks, loop and "
           "instrument map (which samples each program plays, linked to the WAVs in ../music/); the game's song files in original/.",
           "<song>.mid: MIDI type 1, 384 ticks per beat, one MIDI track per song track, the tempo map, the song's MIDI setup "
           "at tick 0 and the loop played twice (marker 'loop' at each loop start). Program numbers are MusyX's, not "
           "General MIDI: load music.sf2 (music_atmos.sf2 for the ambience songs) in the MIDI player to hear the game's own "
           "samples (an approximation: no MusyX envelopes, vibrato or reverb); <song>.txt says which samples each program "
           "plays. original/<song>.song: the untouched song file.",
           "bgm_archive.fsys (type-4 entries); instruments from common.fsys snd_music_* / snd_music_atmos_*",
           evidence=[("Song layout (header, 64-track table, 12-byte track entries, patterns of 6-byte notes and 4-byte controls, pitch / modulation streams, tempo map)",
                      "seqStartPlay (seq.c), GenerateNextTrackEvent (seq_exact_80146E88.c), fn_801485FC (seq_residual_801485FC.c)", "verified (code; all 78 songs parse)"),
                     ("384 ticks per beat", "the sequencer runs 5 x 256 units per 5 ms frame (hardware.c); SetTickDelta divides bpm x time by 40,960,000", "verified (code)"),
                     ("Loop", "a 0xFFFE track entry jumps back; once every track has jumped, song time goes to the header's loop tick (HandleTrackEvents)", "verified (code)"),
                     ("Instrument map", "song id -> MIDI setup; program -> page (BuildTransTab) -> macro / keymap / layer -> start-sample steps (synth.c, synthmacros.c)", "verified (code); each program lists every sample its object can reach"),
                     ("Group and song id per song", "the sound table in common_rel (+0x01 group, +0x04 MusyX song id)", "verified (code + data)"),
                     ("Notes and tempo sound right", "not yet compared with the game by ear", "guess (visual-checks item to add)")],
           docs=["docs/engine/music.md", "docs/modding/music.md"])
    n = 0
    for k, name, t, ident, load in fsys_entries(disc.file("bgm_archive.fsys")):
        if t != 4: continue
        data = load(); s = songlib.parse_song(data)
        how = starts.get(name, [])
        gid, sgid = (how[0][1], how[0][2]) if how else (0, None)
        bank, g, pool = banks.get(gid, (None, None, None))
        setup = g["midi_setups"].get(sgid) if g and sgid is not None else None
        mid = os.path.join(base, f"{name}.mid")
        open(mid, "wb").write(songlib.to_midi(s, name, loops=2, setup=setup))
        L = [f"# {name}", "", f"bgm_archive.fsys entry {k} (id {ident:#010x}), {len(data)} bytes", "",
             "Started by sound ids: " + (", ".join(f"{a} (group {b}, song id {c})" for a, b, c in how) or "none in the sound table"), "",
             songlib.summary(s), ""]
        if g:
            L += [f"## Instruments (MusyX group {gid}, bank '{bank}', MIDI setup of song id {sgid})", ""]
            progs = {}
            for tr in s["tracks"].values():
                ch = tr["channel"] & 15
                if setup: progs.setdefault(ch, set()).add(setup[ch][0])
                for e in tr["entries"]:
                    if e.get("program") is not None: progs.setdefault(ch, set()).add(e["program"])
                    if "pattern" in e:
                        for tt, key, vel in s["patterns"][e["pattern"]]["controls"]:
                            if vel == 0: progs.setdefault(ch, set()).add(key & 0x7F)
            for ch in sorted(progs):
                pages = g["drum_pages"] if ch == 9 else g["normal_pages"]
                for p in sorted(progs[ch]):
                    if p not in pages: L.append(f"- channel {ch}, program {p}: no page (the program change is ignored)"); continue
                    obj = pages[p][0]; kind = {0: "macro", 0x4000: "keymap", 0x8000: "layer"}.get(obj & 0xC000, "?")
                    smp = sorted(object_samples(pool, obj))
                    L.append(f"- channel {ch}{' (drums)' if ch == 9 else ''}, program {p}: {kind} {obj:#06x}, samples " +
                             (", ".join(f"[{x:04X}]({wavs[x]})" if x in wavs else f"{x:04X}" for x in smp) or "none"))
            L.append("")
        txt = os.path.join(base, f"{name}.txt"); open(txt, "w", encoding="utf-8").write("\n".join(L) + "\n")
        note(mid, "bgm_archive", f"{k} ({name})", name, "MIDI (type 1, 384 ticks per beat)")
        note(txt, "bgm_archive", f"{k} ({name})", name, "text: song summary and instrument map")
        keep_original(base, "bgm_archive", f"{k} ({name})", name, 4, data, [mid, txt])
        n += 1
    from colo import soundfont  # one SoundFont per song group: the MIDI files play with the game's samples
    for bank, gid in (("music", 0), ("music_atmos", 1)):
        sf, rep = soundfont.build(com[f"snd_{bank}_proj"](), com[f"snd_{bank}_pool"](), com[f"snd_{bank}_sdir"](),
                                  disc.file(f"sound/snd_{bank}.samp"), gid, f"Colosseum {bank}")
        p = os.path.join(base, f"{bank}.sf2"); open(p, "wb").write(sf)
        note(p, f"common.fsys:snd_{bank}_* + sound/snd_{bank}.samp", f"group {gid}", f"snd_{bank}",
             f"SoundFont 2 ({rep['programs']} programs, {rep['samples']} samples; approximates the MusyX instruments)", str(len(sf)))
    print(f"{n} songs -> {base}")

def cmd_other(disc, out):
    """Stage 5: every entry not written by the other stages, raw, with a note on what it likely is;
    plus the non-FSYS disc files (GBA programs, toc, banner)."""
    base = os.path.join(out, "other")
    readme(base, "Everything not converted by the other stages, kept raw in its original format, one folder per archive; "
           "disc_root/ holds the disc files that are not .fsys archives (except movies and samples).",
           "raw entries named <entry index>_<name>.<kind>; each folder's README.md says what the entries likely are",
           evidence=[("What each entry is", "its FSYS resource type (the floor resource-type table) and its name", "guess unless the linked docs say verified")],
           docs=["docs/engine/fsys-catalog.md", "docs/engine/floor-resources.md", "docs/engine/disc-files.md"])
    skip_arch = lambda a: (a.startswith(("pkx_", "wzx_", "chara_")) or a in ("poke_face", "poke_body", "people_archive", "waza_sequence_global")
                           or re.match(r"[A-Z]\d_", a))
    n = 0
    for arch, arc in archives(disc):
        if skip_arch(arch): continue
        kinds = set()
        for k, name, t, ident, load in fsys_entries(arc):
            if t in (5, 6, 9): continue                    # text, fonts and textures: other stages
            if arch == "field_common" and t == 2: continue  # npcs stage
            if t == 4: continue                             # songs: stage `songs`
            data = load(); n += 1
            if arch == "common" and name.startswith("snd_"): folder = mkdir(os.path.join(out, "audio", "musyx_data"))
            else: folder = mkdir(os.path.join(base, safe(arch))); kinds.add(LIKELY.get(t, f"type {t}"))
            if t in (10, 18) and data[:4] == b"GPT1":  # particle banks: decoded (colo/gpt1.py)
                save_particles(folder, arch, f"{k} ({name})", f"{k:02d}_{name}", data, t); continue
            keep_original(folder, arch, f"{k} ({name})", f"{k:02d}_{name}" if folder.startswith(base) else name, t, data, [],
                          "not converted yet: " + LIKELY.get(t, f"type {t}"))
        if kinds:
            detail(os.path.join(base, safe(arch)), archive_details(disc, arch))
            readme(os.path.join(base, safe(arch)), f"Raw entries of {arch}.fsys that no converter handles yet. Likely contents: " + "; ".join(sorted(kinds)) + ".",
                   "raw game formats (see the file extensions)", f"{arch}.fsys",
                   evidence=[("Entry kinds", "FSYS resource type of each entry", "verified (type); purpose guess")],
                   docs=["docs/engine/fsys-catalog.md"])
    readme(os.path.join(out, "audio", "musyx_data"), "MusyX project, pool and sample-directory files (the instrument and sound-effect definitions).",
           "raw MusyX data", "common.fsys (snd_* entries)", docs=["docs/engine/extractor.md"])
    root = os.path.join(base, "disc_root")
    readme(root, "Disc files that are not .fsys archives: GBA programs sent over the link cable (guess from their first bytes), "
           "GSfsys.toc (archive id -> name), the disc banner.",
           "raw files as on the disc. Movies (movie/*.thp, 280 MB) are not copied: play them from the disc image.",
           evidence=[("GSfsys.toc", "loaded at FSYS init; archive ids -> names", "verified (code)"),
                     (".bin files = GBA programs", "first bytes match a GBA ROM header branch", "guess")],
           docs=["docs/engine/disc-files.md", "docs/engine/gba-link.md", "docs/engine/movies.md"])
    for path in sorted(disc.files):
        if path.endswith((".fsys", ".thp", ".samp")): continue
        conv = []
        if path.endswith(".bnr") and parse_banner(disc.file(path)):  # the disc banner: picture + texts
            bn = parse_banner(disc.file(path)); stem = path.rsplit(".", 1)[0]
            p = os.path.join(mkdir(root), f"{stem}_banner.png"); png(p, *bn["image"]); note(p, path, "(disc file)", path, "PNG (disc banner, 96x32 RGB5A3)", "96x32", "RGB5A3")
            t = os.path.join(root, f"{stem}_banner.txt")
            open(t, "w", encoding="utf-8").write("".join(f"{k}: {v}" + chr(10) for k, v in bn["texts"].items()))
            note(t, path, "(disc file)", path, "text: banner names and description"); conv = [p, t]
        gh = gba_header(disc.file(path)) if path.endswith(".bin") else None
        if gh:  # GBA programs sent over the link cable (pkjb_uploader): their header as text
            t = os.path.join(mkdir(root), f"{path.rsplit('.', 1)[0]}_gba_header.txt")
            open(t, "w", encoding="utf-8").write("".join(f"{k}: {v}" + chr(10) for k, v in gh.items()))
            note(t, path, "(disc file)", path, "text: GBA program header"); conv = conv + [t]
        keep_original(mkdir(root), path, "(disc file)", path.replace("/", "_").rsplit(".", 1)[0], 0, disc.file(path), conv,
                      "non-FSYS disc file", ext=path.rsplit(".", 1)[-1]); n += 1
    print(f"{n} raw entries and files -> {base}")

LAYOUT = """extracted/  (git-ignored: everything here comes from the player's own disc; never commit or push it)

  manifest.csv                    one row per extracted file: path, source archive, entry, original name,
                                  format, size, GameCube texture format, linked file (converted <-> original)
  <any component>/original/       the untouched disc entries behind that folder's converted files
                                  (unpacked from LZSS, otherwise byte-for-byte), with a README table
  pokemon/<NNN>_<Name>/           e.g. pokemon/001_Bulbasaur/ (NNN = internal species index)
      portraits/                  <Name>_normal.png, <Name>_shiny.png (+ _facing_other_way)
      body_pictures/              <Name>_normal.png, <Name>_shiny.png
      model/                      <Name>.pkx (game file), <Name>.dat (HSD model for Blender), <Name>.obj/.mtl
                                  (bind pose, textured); particles/ for species with an ambient effect
      textures/                   the textures inside the model, as PNG
  trainers/<file>_<Class>/        trainer / character battle models: model/ + textures/
  trainers/character_art/         chara_big / chara_small pictures
  npcs/<name>/                    overworld models (NPCs, player, Rui): model/ (.dat + .obj) + textures/
  maps/<prefix>_<Area>/<room>/    e.g. maps/M1_Phenac_City/M1_out/: map/, objects/<name>/, textures/,
                                  collision/<room>.obj (floor, walls, event triggers), cameras/, particles/,
                                  <room>_room_data.txt (entry points, field cameras, lighting), lens_flare/ (T1)
  ui/<archive>/                   menu, UI and screen textures
  effects/moves/<move>/           <part>.txt timeline + <part>_parts/ (particles, models, textures); global/ loops
  text/                           every message bank as UTF-8 text
  fonts/                          font sheets (PNG) + character maps; COVERAGE.md = every drawable character
  scripts/<area>/                 every event script as a readable listing (room, songs, native calls)
  data_tables/                    common_rel tables as CSV (+ species.json)
  audio/<bank>/                   MusyX instrument / effect samples as WAV; audio/songs/ every song as MIDI +
                                  <song>.txt (instrument map); audio/musyx_data/ MusyX files + <bank>_bank.txt
  other/<archive>/                everything else, raw, with a README on what it likely is; other/disc_root/ non-FSYS files
                                  (+ opening_banner.png / .txt)
  raw_entries/                    single entries unpacked by fsys_extract.py (research scratch)

Every folder has a README.md: what it holds, how it was extracted, its in-game use, insights, the
facts the game's own tables give about it ("In the game data": room ids, battles, music, items, move
names), how we know (verified or guess), its exact source archives and entries, formats, the tool,
related docs and open visual checks.
Produced by: python -I tools/local/extract.py "<your disc image>" all
Layout and formats: docs/tools.md ("Extracted folder layout") and docs/engine/extractor.md.
"""

CATALOG = [  # (category, [(relative path, one-line description)]) - design-notes.md note 32 (content catalog), first step
    ("Pokémon", [("pokemon/", "one folder per species: portraits, body pictures, battle model (.pkx, .dat, .obj), textures, animation list"),
                 ("data_tables/menu_faces_decoded.csv", "the portrait table: each menu face id and its poke_face.fsys entry"),
                 ("data_tables/species.json", "species records: types, abilities, base stats, model and portrait ids"),
                 ("data_tables/natures_decoded.csv", "natures: stat effects and purification rates")]),
    ("Moves", [("data_tables/moves_decoded.csv", "every move: type, power, accuracy, PP, priority, effect id, flags, effect animation id"),
               ("effects/moves/", "move animations: timeline per part plus its particles, models and textures")]),
    ("Trainers and battles", [("data_tables/trainers_decoded.csv", "every trainer with class, name and party"),
                              ("data_tables/trainer_pokemon_decoded.csv", "every trainer Pokémon: species, level, item, IVs / EVs, moves"),
                              ("data_tables/battles_decoded.csv", "every battle: type, arena, music, trainers per side"),
                              ("data_tables/trainer_ai_decoded.csv", "the 261 trainer AI profiles: switching, items, move choice, type preferences (developer field names)"),
                              ("data_tables/trainer_ai_scores_decoded.csv", "the battle AI's 553 scoring rules: action, decision, condition, score change (developer labels)"),
                              ("data_tables/battle_kinds_decoded.csv", "the 19 battle kinds and their 25 rule flags (run, items, exp, money, Hyper Mode, ...)"),
                              ("data_tables/arenas_decoded.csv", "battle arenas: name, room, terrain, intro, music, ambient sound"),
                              ("trainers/", "trainer and character battle models (.pkx, .dat, .obj) and character art")]),
    ("Shadow Pokémon", [("data_tables/shadow_pokemon_decoded.csv", "each Shadow id with species, levels, owning trainers and its definition fields")]),
    ("Items", [("data_tables/items_decoded.csv", "every item: name, kind, prices, effects, description"),
               ("data_tables/treasure_decoded.csv", "items placed in the world: room, item, quantity, position")]),
    ("Shops", [("data_tables/shops_decoded.csv", "every shop (the number scripts pass to menuShopOpen): kind, clerk dialogue, stock list with item names")]),
    ("Rooms and maps", [("maps/", "every room by area: map model (.dat, .obj), objects, collision, cameras, particles, room data (entry points, cameras, lighting)"),
                        ("data_tables/rooms_decoded.csv", "the floor table: room id, archive, area, class")]),
    ("NPCs", [("data_tables/npc_placements_decoded.csv", "every NPC placed in a room: model, name, position, scripts"),
              ("npcs/", "overworld models of people and objects (.dat, .obj)")]),
    ("Story and text", [("text/", "every message bank as UTF-8 text"), ("scripts/", "every event script as a readable listing; FLAGS.md = story flags"), ("data_tables/doors_decoded.csv", "every door object: its room, model, animations, collision and open / close flags"), ("data_tables/events_decoded.csv", "every room event trigger: kind, room, event id, script and function, arguments"), ("data_tables/flags_decoded.csv", "every story flag definition: storage, width, initial value, story-chain order"),
                        ("fonts/", "every font: glyph sheet, character map and metrics (line height, spacing); FONTS.md = where each font is used")]),
    ("Music and sound", [("audio/songs/", "every song as MIDI + instrument map; music.sf2 / music_atmos.sf2 to play them"), ("data_tables/menu_sounds_decoded.csv", "the menu sound groups: which sound id plays on cursor move, close, back, open"),
                         ("audio/", "every sound sample as WAV, one folder per bank"), ("audio/musyx_data/", "MusyX banks: programs, macros (readable dumps)")]),
    ("Models", [("pokemon/", "Pokémon battle models"), ("trainers/", "trainer battle models"), ("npcs/", "overworld characters"), ("maps/", "room maps and objects")]),
    ("Effects", [("effects/moves/", "move effects"), ("effects/global/", "sleep / freeze loops (particles)"), ("maps/", "room particles (particles/ in each room)")]),
    ("Movies and menus", [("movies/", "the five videos as MP4 (+ THP originals)"), ("ui/", "menu and screen textures"), ("other/disc_root/", "banner, GBA programs, GSfsys.toc")]),
    ("Everything else", [("other/", "entries no converter handles yet, raw, with notes"), ("manifest.csv", "every file: source archive, entry, format, linked original")]),
]


def write_catalog(out):
    """extracted/CATALOG.md: one page linking every decoded table and asset folder by category (local only, like the
    rest of extracted/; design-notes.md note 32). Missing paths are marked."""
    L = ["# Content catalog (local)", ""] + BANNER + ["Generated by `tools/local/extract.py`; lives only in this git-ignored folder. "
         "Each line: what it is and where. See README.md for the layout and manifest.csv for every file.", ""]
    for cat, items in CATALOG:
        L += [f"## {cat}", ""]
        for rel, what in items:
            mark = "" if os.path.exists(os.path.join(out, rel)) else " *(not extracted yet: run the matching stage)*"
            L.append(f"- [`{rel}`]({rel}): {what}{mark}")
        L.append("")
    pages = catalog_pages(out)
    if pages:
        L += ["## Per-entry pages (where each entry is used)", ""] + [f"- [`{p}`]({p}): {what}" for p, what in pages] + [""]
    open(os.path.join(out, "CATALOG.md"), "w", encoding="utf-8").write("\n".join(L))


def _csv(path):
    import csv
    if not os.path.exists(path): return None
    with open(path, encoding="utf-8") as f: return list(csv.DictReader(f))


def catalog_pages(out):
    """extracted/catalog/<category>.md: one entry per item with where the game uses it (design-notes.md note 32: what it is,
    where it's used, how to mod it). Built only from files this run already wrote (decoded CSVs, script listings), so it
    never reads the disc. Returns [(page path, description)]."""
    import re
    dt = os.path.join(out, "data_tables"); pages = []
    items = _csv(os.path.join(dt, "items_decoded.csv"))
    if not items: return pages
    shops = _csv(os.path.join(dt, "shops_decoded.csv")) or []
    treasure = _csv(os.path.join(dt, "treasure_decoded.csv")) or []
    tpk = _csv(os.path.join(dt, "trainer_pokemon_decoded.csv")) or []
    trainers = _csv(os.path.join(dt, "trainers_decoded.csv")) or []
    # which room opens which shop: menuShopOpen(n) in the script listings
    shop_rooms = {}
    sdir = os.path.join(out, "scripts")
    for dp, dn, fn in os.walk(sdir):
        for f_ in fn:
            if not f_.endswith(".txt"): continue
            for m in re.finditer(r"menuShopOpen\((\d+)\)", open(os.path.join(dp, f_), encoding="utf-8", errors="replace").read()):
                shop_rooms.setdefault(int(m.group(1)), set()).add(f_.split("__")[0])
    owner = {}  # trainer-Pokemon index -> "class name"
    for t in trainers:
        try: first = int(t["first_trainer_pokemon"])
        except (KeyError, ValueError): continue
        for k in range(first, first + 6): owner.setdefault(k, f"{t.get('class', '')} {t.get('name', '')}".strip())
    use = {}
    for s in shops:
        for ent in s["items (price; coupons at the exchanges)"].split("; "):
            if not ent: continue
            name, _, price = ent.rpartition(" (")
            where = ", ".join(sorted(shop_rooms.get(int(s["shop"]), []))) or "no room script (menu or unused)"
            use.setdefault(name, []).append(f"sold in shop {s['shop']} ({where}) for {price.rstrip(')')}")
    for t in treasure:
        use.setdefault(t["item"], []).append(f"found in {t['room'] or 'room ' + t['room_id']} (x{t['quantity']})")
    held = {}
    for p in tpk:
        if p.get("held_item"): held.setdefault(p["held_item"], set()).add(owner.get(int(p["index"]), f"trainer Pokémon {p['index']}"))
    for name, who in held.items():
        use.setdefault(name, []).append(f"held by Pokémon of {len(who)} trainer(s): " + ", ".join(sorted(who)[:6]) + (" ..." if len(who) > 6 else ""))
    d = mkdir(os.path.join(out, "catalog"))
    L = ["# Items: where each one is used (local)", ""] + BANNER + [
        "From the decoded tables (items, shops, treasure, trainer Pokémon) and the script listings. How to mod them: "
        "`docs/modding/shops.md` (stock), `docs/modding/trainers.md` (held items); item fields: `docs/engine/extractor.md`.", ""]
    unused = 0
    for it in items:
        u = use.get(it["name"], [])
        if not u: unused += 1
        L += [f"## {it['item_id']}: {it['name'] or '(no name)'}", "",
              f"Kind {it['kind']}, price {it['price']}, coupon price {it['coupon_price']}. {it['description']}".strip(), ""]
        L += [f"- {x}" for x in u] or ["- not sold, placed or held anywhere in these tables (may still come from scripts or events)"]
        L.append("")
    L.insert(len(BANNER) + 2, f"{len(items)} items; {len(items) - unused} sold, placed or held somewhere; {unused} found in none of these tables.\n")
    open(os.path.join(d, "items.md"), "w", encoding="utf-8").write("\n".join(L))
    pages.append(("catalog/items.md", "every item with the shops that sell it (and their rooms), where it lies in the world, and which trainers' Pokémon hold it"))
    # trainers: party, the battles they take part in (battle table side slots), and the rooms whose scripts start those
    # battles with a literal number (fightStartEncount / fieldStartEncount); computed battle numbers are not resolved
    battles = _csv(os.path.join(dt, "battles_decoded.csv")) or []
    if trainers and battles:
        started = {}
        for dp, dn, fn in os.walk(sdir):
            for f_ in fn:
                if not f_.endswith(".txt"): continue
                txt_ = open(os.path.join(dp, f_), encoding="utf-8", errors="replace").read()
                for m in re.finditer(r"(?:fight|field)StartEncount\((\d+)", txt_):
                    started.setdefault(int(m.group(1)), set()).add(f_.split("__")[0])
        in_battle = {}
        for b in battles:
            for part in b["side_trainers"].split("; "):
                tid = part.split(":")[0].strip()
                if tid.isdigit(): in_battle.setdefault(int(tid), []).append(b)
        L = ["# Trainers: party and where you meet them (local)", ""] + BANNER + [
            "From the decoded trainer, trainer-Pokémon and battle tables, and the room scripts that start a battle by a "
            "literal number (battles started with a computed number are not resolved here). How to mod them: "
            "`docs/modding/trainers.md`.", ""]
        met = 0
        for t in trainers:
            tid = int(t["trainer"]); bl = in_battle.get(tid, [])
            rooms = sorted({r for b in bl for r in started.get(int(b["battle"]), [])})
            if rooms: met += 1
            L += [f"## {tid}: {t['class']} {t['name']}".rstrip(), "", f"Party: {t['party'] or '(empty)'}", ""]
            L += [f"- battle {b['battle']} (type {b['type']}, arena {b['arena_name'] or b['arena']}, music {b['music_song'] or b['music_sound_id']})"
                  + (f": started in {', '.join(sorted(started[int(b['battle'])]))}" if int(b["battle"]) in started else "")
                  for b in bl] or ["- in no battle record"]
            L.append("")
        L.insert(len(BANNER) + 2, f"{len(trainers)} trainers; {met} met in a room whose script starts their battle by number.\n")
        open(os.path.join(d, "trainers.md"), "w", encoding="utf-8").write("\n".join(L))
        pages.append(("catalog/trainers.md", "every trainer with their party, the battles they appear in (arena, music) and the rooms whose scripts start them"))
    return pages


def default_out():
    """One extracted folder for every worktree (snatcherLeo, 2026-10-16): $ORRE_EXTRACTED if set, else the main checkout's
    extracted/ (the parent of git's shared .git folder, so worktrees like ../formats-d write to
    ../pkmn_colosseum/extracted), else ./extracted."""
    if os.environ.get("ORRE_EXTRACTED"): return os.environ["ORRE_EXTRACTED"]
    import subprocess
    here = os.path.dirname(os.path.abspath(__file__))
    try:
        r = subprocess.run(["git", "-C", here, "rev-parse", "--git-common-dir"], capture_output=True, text=True, timeout=20)
        common = r.stdout.strip()
        if r.returncode == 0 and common:
            if not os.path.isabs(common): common = os.path.join(here, common)
            return os.path.join(os.path.dirname(os.path.abspath(common)), "extracted")
    except (OSError, subprocess.SubprocessError):
        pass
    return "extracted"

def main():
    if len(sys.argv) < 3: print(__doc__); sys.exit(1)
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else default_out()
    print(f"output folder: {os.path.abspath(out)}")
    global REPO  # README links point at the docs of the checkout that owns the output folder
    if os.path.isdir(os.path.join(os.path.dirname(os.path.abspath(out)), "docs")): REPO = os.path.dirname(os.path.abspath(out))
    disc = Disc(sys.argv[1]); what = sys.argv[2]
    global DISC; DISC = disc
    mkdir(out)
    if os.path.exists(os.path.join(out, "README.txt")): os.remove(os.path.join(out, "README.txt"))
    readme(os.path.join(out, "raw_entries"), "Single .fsys entries unpacked by hand for research (scratch).",
           "raw unpacked entry, named <archive>__<index>_<name>.t<type> (type = the FSYS resource type)",
           "any archive (see the file names)", docs=["docs/engine/disc-files.md", "docs/tools.md"])
    cmds = {"pokemon": cmd_pokemon, "trainers": cmd_trainers, "npcs": cmd_npcs, "maps": cmd_maps, "ui": cmd_ui,
            "effects": cmd_effects, "text": cmd_text, "fonts": cmd_fonts, "scripts": cmd_scripts, "tables": cmd_tables, "audio": cmd_audio, "songs": cmd_songs, "movies": cmd_movies, "other": cmd_other}
    for name in (cmds if what == "all" else [what]): cmds[name](disc, out)
    if OBJ_FAILED: print(f"{len(OBJ_FAILED)} models not converted to OBJ (kept as .dat):", *OBJ_FAILED[:20], sep="\n  ")
    finish_readmes.out = os.path.abspath(out)
    finish_readmes()
    finish_originals()
    for i, row in enumerate(MANIFEST):
        row = MANIFEST[i] = list(row) + [""] * (8 - len(row))
        if not row[7] and row[0] in LINKS: row[7] = LINKS[row[0]]
    # manifest: keep rows from earlier runs, replace rows for files written now
    mp = os.path.join(out, "manifest.csv"); header = "path,source_archive,entry,original_name,format,size,gamecube_texture_format,linked_file\n"
    new_paths = {r[0] for r in MANIFEST}; kept = []
    if os.path.exists(mp) and what != "all":
        with open(mp, encoding="utf-8") as f:
            next(f, None)
            for line in f:
                path = line.split('","', 1)[0].lstrip('"')
                if path not in new_paths and os.path.exists(path): kept.append(line)
    with open(mp, "w", encoding="utf-8", newline="") as f:
        f.write(header); f.writelines(kept)
        for row in MANIFEST: f.write(",".join('"' + str(c).replace('"', "'") + '"' for c in row) + "\n")
    write_top_readme(out)
    write_catalog(out)

if __name__ == "__main__":
    main()
