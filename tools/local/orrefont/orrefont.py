"""Builds OrreFont Colosseum, OrrePort's installable font file, from the player's own discs (the project's build helper,
2026-11-16; tools/local/orrefont/README.md; docs/port/font-renderer.md "Start here: OrreFont").

Why: snatcherLeo decided to ship OrreFont as a font file in the portable folder's bin\\Fonts\\ ("just for fun, our OrreFont as
a font file ... in case anyone wants to install on their computer", 2026-11-15; named "OrreFont Colosseum" on
2026-11-16; a lawyer's read cleared it). It holds the letters OrrePort draws: the game's own letters traced sharp by the
game's own tracer, as smooth outlines. It is not used by the game's renderer (which traces the player's disc at run
time); it is for players and modders to install, and for Studio's previews later.

How, in order (the `build` command does all of it):
  1. the glyph dump: every glyph of the US disc's and the Japanese disc's fonts (tools/local/kanji_trace_check.gather,
     the reading the kanji review uses), with each glyph's vertical offset, into <work>/glyphs.bin;
  2. the letters of a style: the US font for Latin letters and symbols, the Japanese disc's font of the same size for
     Japanese characters (the game's own split, pc_text_atlas.cpp `is_japanese`: U+3000-30FF, U+3400-9FFF,
     U+FF00-FFEF), each falling back to the other disc;
  3. the trace and the outlines: orrefont_trace.cpp (built here with MSYS2's g++) includes the game's own
     pc_text_atlas.cpp, traces each letter with the tracer the game uses for it, and fits lines and curves to the trace;
  4. the font: fontTools writes a TrueType file (quadratic curves from the fitted cubics, within one font unit), with
     the game's metrics (one game pixel = 100 units, the em = the font's cell height, the baseline from the font's own
     'H', advances = the game's widths, the space = half the cell width, no kerning: the game has none);
  5. the checks (`check`): read back with fontTools, every glyph drawn by FreeType (Pillow), and the drawn letters
     compared with the game's own trace at 8x; a specimen sheet into the work folder.

    python -I tools/local/orrefont/orrefont.py build --us "<US disc image>" --jp "<Japanese disc image>" --work <scratch>
    python -I tools/local/orrefont/orrefont.py check --work <scratch>

Options: --style regular|small|both (default regular), --keep-rare (keep the kanji the game draws from Noto, as the
game's own picture traced), --jobs N (threads, default 12), --dest <folder> (default port/ui/traced/fonts/).

Runs under `python -I` (isolated: the repo's own tools only): fontTools and Pillow live in the user's site-packages,
which -I hides, so that one folder is added to the path explicitly below (snatcherLeo approved fontTools, 2026-11-15/16; it
was installed with pip --user). Nothing from the disc enters the repo except the finished font, which is a traced
shape: it carries the generated-from-disc marker in a private table and the commit hook allows it in the traced-shapes
folder only (tools/local/hooks/check_game_data.py).
Platforms: the Python is the same on every OS. The tracer build (orrefont_trace.cpp) uses MSYS2 MinGW and build-pc's
SDL3 / FreeType on Windows; on Linux / macOS any C++20 compiler with SDL3 and FreeType from their install (likely; the
command in build_tracer would need their paths). XD: OrreFont XD would be the same tool on XD's discs (xd-insights.md
3b): the Latin letters are the same pictures; its kanji come from XD's own Japanese disc.
"""
import argparse
import json
import os
import site
import struct
import subprocess
import sys
import time

# fontTools and Pillow come from the user's site-packages, which python -I leaves off the path (see above)
sys.path.append(site.getusersitepackages())
REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "local"))

MARKER = "ORREPORT-" + "GENERATED-FROM-DISC"  # built so this file itself doesn't hold the marker text
VERSION = "1.000"
BUILD_DATE = (2026, 11, 16)  # the file's fixed date stamp, so a rebuild with the same inputs gives the same bytes
UNITS_PER_PX = 100           # one game pixel = 100 font units: the em is the cell height times 100
MSYS = "C:/msys64/mingw64/bin"

# The styles. us / jp: (font id, cell height) on each disc. line: the game's line height (cell + line gap, gs_msg.c
# GSmsgSetFontInfo: 11 for font 0, 6 for font 1). Regular is font 0, the 22-px dialogue font, the main look.
STYLES = {
    "regular": {"family": "OrreFont Colosseum", "us": (0, 22), "jp": (0, 22), "line": 33,
                "file": "OrreFont Colosseum.ttf", "what": "the 22-pixel dialogue font"},
    "small": {"family": "OrreFont Colosseum Small", "us": (1, 18), "jp": (1, 18), "line": 24,
              "file": "OrreFont Colosseum Small.ttf", "what": "the 18-pixel menu font"},
}

# The name table's words (checked by `check`: none of the banned words, no claim to the original design)
COPYRIGHT = ("Outlines made by the OrrePort project: letter shapes traced from the lettering of a player's own copy "
             "of the game. No claim is made to the original letter designs.")
DESCRIPTION = ("{family}: the letters of the game Colosseum ({what}, with the kana and kanji of its Japanese "
               "release), traced into smooth outlines by OrrePort, a fan-made PC port, with the same tracer its "
               "renderer uses. Letter shapes traced by OrrePort from a player's own copy of the game.")
# The font's licence line (name ID 13). Why it's there (snatcherLeo, 2026-11-16: "A, just include documentation on why it's in
# there in the first place"): without one, anyone who installs the font can't tell what they may do with it. Option A,
# a short custom notice, was chosen over the SIL Open Font License (which grants rights to the letter designs, and
# those designs aren't OrrePort's to grant) and over no line at all. "Not to be sold" follows snatcherLeo's standing rule that
# nothing derived from the game is ever sold (his Patreon plan: his time and tools only). README.md "The licence line".
LICENSE = ("Free to use in personal and fan projects, and to share unchanged. Not to be sold. Provided as is, without "
           "warranty of any kind.")
BANNED = ("pokémon", "pokemon", "nintendo", "game freak", "gamefreak", "genius sonority", "creatures inc",
          " ai ", "artificial intelligence")


def is_japanese(c):
    """the game's routing to the kanji tracer (pc_text_atlas.cpp is_japanese)"""
    return 0x3000 <= c <= 0x30FF or 0x3400 <= c <= 0x9FFF or 0xFF00 <= c <= 0xFFEF


def group(c):
    if 0x4E00 <= c <= 0x9FFF or 0x3400 <= c <= 0x4DBF or c == 0x3005:
        return "kanji"
    if 0x3040 <= c <= 0x30FF:
        return "kana"
    if 0x3000 <= c <= 0x303F or 0xFF00 <= c <= 0xFFEF:
        return "CJK punctuation and full-width"
    if c < 0x80 and (chr(c).isalnum()):
        return "ASCII letters and digits"
    if c < 0x250 and chr(c).isalpha():
        return "accented Latin"
    return "punctuation and symbols"


# --- 1. the glyph dump ------------------------------------------------------------------------------------------------

def dump(us_iso, jp_iso, work):
    """every glyph of both discs into <work>/glyphs.bin (orrefont_trace.cpp's input) and glyphs.json (the index)"""
    import kanji_trace_check as K
    rows = []
    for disc, iso in ((0, us_iso), (1, jp_iso)):
        g = K.gather(iso)
        for (fid, cell, ch) in sorted(g):
            gl = g[(fid, cell, ch)]
            rows.append((disc, fid, cell, ch, gl))
        print(f"  {'US' if disc == 0 else 'Japanese'} disc: {len(g)} glyphs")
    index = []
    with open(os.path.join(work, "glyphs.bin"), "wb") as f:
        f.write(struct.pack("<I", len(rows)))
        for disc, fid, cell, ch, gl in rows:
            w, h = gl["width"], len(gl["pixels"])
            f.write(struct.pack("<BHBHHHb", disc, fid, cell, ch, w, h, gl["y_offset"]))
            f.write(bytes(v for row in gl["pixels"] for v in row[:w]))
            index.append({"disc": disc, "font": fid, "cell": cell, "char": ch, "w": w, "h": h,
                          "yoff": gl["y_offset"], "lv": [v for row in gl["pixels"] for v in row[:w]]})
    with open(os.path.join(work, "glyphs.json"), "w", encoding="utf-8") as f:
        json.dump({"us": os.path.basename(us_iso), "jp": os.path.basename(jp_iso), "glyphs": index}, f)
    return index


def load_index(work):
    with open(os.path.join(work, "glyphs.json"), encoding="utf-8") as f:
        return json.load(f)["glyphs"]


# --- 2. the letters of a style --------------------------------------------------------------------------------------

def baseline_of(index, disc, fid, cell):
    """the font's baseline from the line's top: the bottom of its 'H' (rows at least half dark), as the renderer
    measures it (gs_msg.c orre_font_metrics)"""
    for g in index:
        if g["disc"] == disc and g["font"] == fid and g["cell"] == cell and g["char"] == ord("H"):
            rows = [y for y in range(g["h"]) if any(v >= 8 for v in g["lv"][y * g["w"]:(y + 1) * g["w"]])]
            return g["yoff"] + max(rows) + 1
    raise SystemExit(f"no 'H' in font {fid} ({cell} px) on disc {disc}")


def pick(index, style):
    """{code point: glyph index} for a style: Japanese characters from the Japanese disc first, the rest from the US
    disc first, each falling back to the other; the space is not a glyph (the game never looks it up)"""
    st = STYLES[style]
    by = {}
    for i, g in enumerate(index):
        key = (g["disc"], g["font"], g["cell"])
        if key == (0,) + st["us"] or key == (1,) + st["jp"]:
            by.setdefault((g["disc"], g["char"]), i)
    chars = sorted({c for (_, c) in by})
    out = {}
    for c in chars:
        if c == 0x20:
            continue
        order = (1, 0) if is_japanese(c) else (0, 1)
        for d in order:
            if (d, c) in by:
                out[c] = by[(d, c)]
                break
    return out


# --- 3. the tracer --------------------------------------------------------------------------------------------------

def build_tracer(work):
    """compile orrefont_trace.cpp (it includes the game's pc_text_atlas.cpp) when it or the tracer changed"""
    exe = os.path.join(work, "orrefont_trace.exe")
    srcs = [os.path.join(REPO, "tools", "local", "orrefont", "orrefont_trace.cpp"),
            os.path.join(REPO, "port", "src", "pc_text_atlas.cpp")]
    if os.path.exists(exe) and all(os.path.getmtime(exe) > os.path.getmtime(s) for s in srcs):
        return exe
    deps = os.path.join(REPO, "build-pc", "_deps")
    cmd = [os.path.join(MSYS, "g++.exe"), "-O2", "-std=c++20", "-I" + REPO,
           "-I" + os.path.join(REPO, "extern", "aurora", "include"),
           "-I" + os.path.join(deps, "sdl3_prebuilt-src", "include"),
           "-I" + os.path.join(deps, "freetype-build", "include"), "-I" + os.path.join(deps, "freetype-src", "include"),
           srcs[0], "-o", exe, "-L" + os.path.join(deps, "sdl3_prebuilt-src", "lib"), "-lSDL3",
           os.path.join(deps, "freetype-build", "libfreetype.a"), os.path.join(deps, "png-build", "libpng16.dll.a"),
           os.path.join(deps, "zlib-build", "libzlib.dll.a")]
    env = dict(os.environ, PATH=MSYS + os.pathsep + os.environ.get("PATH", ""))
    print("  building the tracer (orrefont_trace.cpp + the game's pc_text_atlas.cpp)")
    r = subprocess.run(cmd, cwd=REPO, env=env, capture_output=True, text=True, errors="replace")
    if r.returncode != 0:
        raise SystemExit("the tracer didn't build:\n" + r.stderr[-4000:])
    return exe


def tracer_env():
    """the tracer's DLLs: build-pc's SDL3, libpng16 and zlib, and MSYS2's runtime"""
    deps = os.path.join(REPO, "build-pc", "_deps")
    dirs = [MSYS, os.path.join(deps, "sdl3_prebuilt-src", "bin"), os.path.join(deps, "sdl3_prebuilt-src", "lib"),
            os.path.join(deps, "png-build"), os.path.join(deps, "zlib-build"), os.path.join(REPO, "build-pc", "firstlink")]
    return dict(os.environ, PATH=os.pathsep.join(dirs + [os.environ.get("PATH", "")]))


def used_kanji(work):
    """the kanji the Japanese game's text uses (the rare-kanji rule's "used"), from the extraction's text banks with
    the kanji review's freq.py; the game computes the same set from the disc's messages (871 here, 872 in the game,
    font-renderer.md "Rare kanji from Noto")"""
    src = os.path.join(REPO, "extracted", "regions", "GC6J01")
    out = os.path.join(work, "kanji_freq.tsv")
    if os.path.exists(out):
        return out
    if not os.path.isdir(src):
        print("  note: no extracted/regions/GC6J01: only D-level kanji count as drawn from Noto")
        return None
    subprocess.run([sys.executable, "-I", os.path.join(REPO, "tools", "local", "kanji_review", "freq.py"), src, work],
                   check=True)
    return out


def trace(work, style, chars, jobs, crop=False):
    """trace a style's letters into <work>/<style>/ (outlines.txt, trace8.bin)"""
    exe = build_tracer(work)
    out = os.path.join(work, style)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "jobs.txt"), "w") as f:
        for c in sorted(chars):
            f.write(f"{chars[c]} {'J' if is_japanese(c) else 'L'}\n")
    args = [exe, os.path.join(work, "glyphs.bin"), os.path.join(out, "jobs.txt"), out, str(jobs),
            "--fonts=" + os.path.join(REPO, "..", "_external", "fonts", "noto-cjk")]
    used = used_kanji(work)
    if used:
        args.append("--used=" + used)
    if crop:
        args.append("--crop")
    t0 = time.time()
    r = subprocess.run(args, cwd=REPO, env=tracer_env(), capture_output=True, text=True, errors="replace")
    if r.returncode != 0:
        raise SystemExit("the tracer failed:\n" + r.stdout[-2000:] + r.stderr[-2000:])
    print("  " + r.stdout.strip() + f" ({time.time() - t0:.0f} s)")
    return read_outlines(out)


def read_outlines(work):
    """{glyph index: {tracer, noto, borrowed, kept, spill, contours: [[('M'|'L'|'B', points)...]]}}"""
    out = {}
    cur = None
    with open(os.path.join(work, "outlines.txt")) as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            if p[0] == "G":
                cur = {"tracer": p[2], "noto": p[3] == "1", "borrowed": p[4] == "1", "kept": p[5] == "1",
                       "spill": float(p[6]), "contours": []}
                out[int(p[1])] = cur
            elif p[0] == "M":
                cur["contours"].append([("M", [(float(p[1]), float(p[2]))])])
            elif p[0] == "L":
                cur["contours"][-1].append(("L", [(float(p[1]), float(p[2]))]))
            elif p[0] == "B":
                v = list(map(float, p[1:]))
                cur["contours"][-1].append(("B", [(v[0], v[1]), (v[2], v[3]), (v[4], v[5])]))
    return out


# --- 4. the font ----------------------------------------------------------------------------------------------------

def glyph_name(c):
    return "space" if c == 0x20 else ("uni%04X" % c)


def build_font(style, index, chars, outlines, dest, keep_rare):
    from fontTools.fontBuilder import FontBuilder
    from fontTools.pens.areaPen import AreaPen
    from fontTools.pens.cu2quPen import Cu2QuPen
    from fontTools.pens.ttGlyphPen import TTGlyphPen
    from fontTools.ttLib import newTable
    from fontTools.ttLib.tables.DefaultTable import DefaultTable

    st = STYLES[style]
    fid, cell = st["us"]
    upm = cell * UNITS_PER_PX
    base = baseline_of(index, 0, fid, cell)
    # the line: from the line's top (the baseline's height above it) to the next line's top (the game's line height)
    ascent, descent = base * UNITS_PER_PX, -(st["line"] - base) * UNITS_PER_PX
    cell_w = cell  # every main font's cell is square (fonts.md)
    left_out = []
    glyphs, advances, cmap = {}, {}, {}

    def to_units(g, x, y):
        return (x * UNITS_PER_PX, (base - (g["yoff"] + y)) * UNITS_PER_PX)

    # .notdef: the game's own "missing letter", a box the size of the cell (msgSetChar), drawn as an outline
    pen = TTGlyphPen(None)
    m, t = 100, 120
    w_nd = (cell_w + 2) * UNITS_PER_PX
    for (x0, y0, x1, y1, ccw) in ((m, descent // 3, w_nd - m, ascent - 300, False),
                                  (m + t, descent // 3 + t, w_nd - m - t, ascent - 300 - t, True)):
        pts = [(x0, y0), (x0, y1), (x1, y1), (x1, y0)]
        if ccw:
            pts.reverse()
        pen.moveTo(pts[0])
        for q in pts[1:]:
            pen.lineTo(q)
        pen.closePath()
    glyphs[".notdef"], advances[".notdef"] = pen.glyph(), w_nd
    # the space: never a glyph in the game; it advances half the cell width (msgSetChar); the no-break space too
    for c in (0x20, 0xA0):
        n = glyph_name(c)
        glyphs[n], advances[n], cmap[c] = TTGlyphPen(None).glyph(), (cell_w // 2) * UNITS_PER_PX, n
    stats = {"groups": {}, "rare_left_out": [], "spill_max": 0.0, "not_kept": []}
    for c in sorted(chars):
        gi = chars[c]
        g, o = index[gi], outlines[gi]
        if o["noto"] and not keep_rare:
            stats["rare_left_out"].append(c)
            continue
        if not o["kept"]:
            stats["not_kept"].append(c)
        stats["spill_max"] = max(stats["spill_max"], o["spill"])
        n = glyph_name(c)
        conts = [[(op, [to_units(g, x, y) for (x, y) in pts]) for op, pts in cont] for cont in o["contours"]]
        # orientation: TrueType draws outer outlines clockwise (y up); the tracer's loops all have the ink on one side,
        # so the sign of the whole area (measured on the curves themselves) says which way they all run
        def draw(pen):
            for cont in conts:
                for op, pts in cont:
                    if op == "M":
                        pen.moveTo(pts[0])
                    elif op == "L":
                        pen.lineTo(pts[0])
                    else:
                        pen.curveTo(*pts)
                pen.closePath()
        ap = AreaPen(None)
        draw(ap)
        tt = TTGlyphPen(None)
        draw(Cu2QuPen(tt, max_err=1.0, reverse_direction=ap.value > 0))  # fontTools' area > 0: counter-clockwise
        glyphs[n] = tt.glyph()
        advances[n] = g["w"] * UNITS_PER_PX
        cmap[c] = n
        grp = group(c)
        stats["groups"][grp] = stats["groups"].get(grp, 0) + 1
    # the game's yen sign sits at the backslash's code (fonts.md "Special symbols"): also reachable as U+00A5
    if 0x5C in cmap and 0xA5 not in cmap:
        cmap[0xA5] = cmap[0x5C]
    order = [".notdef"] + sorted(n for n in glyphs if n != ".notdef")
    fb = FontBuilder(upm, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(glyphs)
    glyf = fb.font["glyf"]
    fb.setupHorizontalMetrics({n: (advances[n], getattr(glyf[n], "xMin", 0)) for n in order})
    fb.setupHorizontalHeader(ascent=ascent, descent=descent, lineGap=0)
    ymax = max([getattr(glyf[n], "yMax", 0) for n in order] + [ascent])
    ymin = min([getattr(glyf[n], "yMin", 0) for n in order] + [descent])
    family = st["family"]
    desc = DESCRIPTION.format(family=family, what=st["what"])
    fb.setupNameTable({
        "copyright": COPYRIGHT,
        "familyName": family,
        "styleName": "Regular",
        "uniqueFontIdentifier": f"{family} Regular {VERSION}; OrrePort",
        "fullName": family,
        "version": f"Version {VERSION}",
        "psName": family.replace(" ", "") + "-Regular",
        "manufacturer": "OrrePort",
        "description": desc,
        "licenseDescription": LICENSE,  # name ID 13; why: see LICENSE above
    }, mac=False)
    fb.setupOS2(version=4, achVendID="NONE", usWeightClass=400, usWidthClass=5, fsType=0,
                fsSelection=0x40 | 0x80,  # Regular, USE_TYPO_METRICS (the typo metrics are the game's line)
                sTypoAscender=ascent, sTypoDescender=descent, sTypoLineGap=0,
                usWinAscent=ymax, usWinDescent=-ymin,
                sxHeight=_height(index, 0, fid, cell, "x", base), sCapHeight=_height(index, 0, fid, cell, "H", base),
                usDefaultChar=0, usBreakChar=0x20, usMaxContext=0)
    fb.setupPost(keepGlyphNames=True)
    stamp = _since_1904(*BUILD_DATE)
    fb.setupHead(unitsPerEm=upm, fontRevision=float(VERSION), created=stamp, modified=stamp, lowestRecPPEM=8)
    fb.setupMaxp()
    os2 = fb.font["OS/2"]
    os2.recalcUnicodeRanges(fb.font)
    # code pages set by hand: Latin 1 (bit 0) and Japanese (bit 17). fontTools' guess left out Latin 1 (the game's
    # Latin-1 set is partial: no cent, pound or section signs) and added Big5 (which the kanji's coverage tripped);
    # Windows picks fonts for a script by these bits, so Western text must be listed
    os2.ulCodePageRange1, os2.ulCodePageRange2 = (1 << 0) | (1 << 17), 0
    os2.recalcAvgCharWidth(fb.font)
    gasp = newTable("gasp")
    gasp.version = 1
    gasp.gaspRange = {0xFFFF: 0x000F}  # smooth at every size (the outlines carry no hinting)
    fb.font["gasp"] = gasp
    # the generated-from-disc marker, in a private table nothing reads: the commit hook finds it, so a copy of this
    # file outside the traced-shapes folder is refused (tools/local/hooks/check_game_data.py)
    mark = DefaultTable("ORRE")
    mark.data = (MARKER + ": OrrePort's traced outlines").encode("ascii")
    fb.font["ORRE"] = mark
    fb.font.recalcTimestamp = False
    os.makedirs(dest, exist_ok=True)
    path = os.path.join(dest, st["file"])
    fb.save(path)
    stats.update({"path": path, "glyphs": len(order), "upm": upm, "baseline": base, "ascent": ascent,
                  "descent": descent, "bytes": os.path.getsize(path)})
    return stats


def _since_1904(y, m, d):
    import calendar
    return calendar.timegm((y, m, d, 0, 0, 0)) + 2082844800  # seconds from 1904-01-01 (the font format's epoch)


def _height(index, disc, fid, cell, ch, base):
    for g in index:
        if g["disc"] == disc and g["font"] == fid and g["cell"] == cell and g["char"] == ord(ch):
            rows = [y for y in range(g["h"]) if any(v >= 8 for v in g["lv"][y * g["w"]:(y + 1) * g["w"]])]
            return (base - (g["yoff"] + min(rows))) * UNITS_PER_PX
    return 0


# --- 5. the checks and the specimen -----------------------------------------------------------------------------------

def read_trace8(work):
    out = {}
    with open(os.path.join(work, "trace8.bin"), "rb") as f:
        while True:
            h = f.read(8)
            if len(h) < 8:
                break
            gi, w, hh = struct.unpack("<IHH", h)
            out[gi] = (w, hh, f.read(w * 8 * hh * 8))
    return out


def validate(path):
    """fontTools reads it back, every table decompiles and compiles again, the names hold no banned word, contours
    are closed and run the TrueType way; returns a list of problems (empty = good)"""
    from fontTools.pens.areaPen import AreaPen
    from fontTools.ttLib import TTFont
    probs = []
    f = TTFont(path)
    for tag in f.keys():
        if tag == "GlyphOrder":
            continue
        try:
            tab = f[tag]
            tab.compile(f)
        except Exception as e:  # noqa: BLE001 (any failure is a problem to report)
            probs.append(f"table {tag}: {e}")
    for rec in f["name"].names:
        s = " " + rec.toUnicode().lower() + " "
        for b in BANNED:
            if b in s:
                probs.append(f"name {rec.nameID}: holds '{b.strip()}'")
    gs = f.getGlyphSet()
    wrong = []
    for n in f.getGlyphOrder():
        if n == ".notdef":
            continue
        p = AreaPen(gs)
        gs[n].draw(p)
        if p.value > 0:  # fontTools' area: negative for TrueType's clockwise outer outlines
            wrong.append(n)
    if wrong:
        probs.append(f"{len(wrong)} glyphs run the wrong way: {wrong[:8]}")
    if f["maxp"].numGlyphs != len(f.getGlyphOrder()):
        probs.append("maxp's glyph count is off")
    return probs, f


def check(work, styles):
    from PIL import Image, ImageChops, ImageDraw, ImageFont, ImageStat
    index = load_index(work)
    with open(os.path.join(work, "build.json"), encoding="utf-8") as f:
        built = json.load(f)
    report = {}
    for style in styles:
        info = built[style]
        t8 = read_trace8(os.path.join(work, style))
        path = info["path"]
        probs, tt = validate(path)
        cmap = tt.getBestCmap()
        st = STYLES[style]
        cell = st["us"][1]
        base = info["baseline"]
        font = ImageFont.truetype(path, size=cell * 8, layout_engine=ImageFont.Layout.BASIC)
        rows = []
        for c, gi in info["chars"].items():
            c, gi = int(c), int(gi)
            if c not in cmap or gi not in t8:
                continue
            g = index[gi]
            w, h, data = t8[gi]
            M = 4  # game pixels of room around the glyph box
            im = Image.new("L", ((w + 2 * M) * 8, (cell + 2 * M + 8) * 8), 0)
            ImageDraw.Draw(im).text((M * 8, (M + base) * 8), chr(c), font=font, fill=255, anchor="ls")
            box = (M * 8, (M + g["yoff"]) * 8, (M + w) * 8, (M + g["yoff"] + h) * 8)
            ours = im.crop(box)
            game = Image.frombytes("L", (w * 8, h * 8), data)
            tot = ImageStat.Stat(im).sum[0] / 255.0
            inbox = ImageStat.Stat(ours).sum[0] / 255.0
            diff = ImageStat.Stat(ImageChops.difference(ours, game)).mean[0]
            half = lambda i: i.point(lambda v: 255 if v >= 128 else 0)  # noqa: E731
            gb, ob = half(game), half(ours)
            xor = ImageStat.Stat(ImageChops.difference(gb, ob)).sum[0] / 255.0
            ink = ImageStat.Stat(gb).sum[0] / 255.0
            # the outline's length: 8x pixels next to one of the other kind (the usual pi / 4 correction for diagonal
            # edges); the edges moved = the pixels that differ / that length
            edge = (ImageStat.Stat(ImageChops.difference(gb.crop((1, 0, w * 8, h * 8)),
                                                          gb.crop((0, 0, w * 8 - 1, h * 8)))).sum[0]
                    + ImageStat.Stat(ImageChops.difference(gb.crop((0, 1, w * 8, h * 8)),
                                                            gb.crop((0, 0, w * 8, h * 8 - 1)))).sum[0]) / 255.0
            shift = xor / max(edge * 0.785, 1) / 8.0  # game pixels
            rows.append({"char": c, "group": group(c), "mad": diff, "xor": xor / max(ink, 1), "shift": shift,
                         "outside": (tot - inbox) / max(tot, 1)})
        report[style] = {"problems": probs, "rows": rows}
        groups = {}
        for r in rows:
            groups.setdefault(r["group"], []).append(r)
        print(f"{st['family']}: {len(rows)} letters compared with the game's trace at 8x"
              + ("" if not probs else f"; PROBLEMS: {probs}"))
        for grp, rs in sorted(groups.items()):
            md = sorted(r["mad"] for r in rs)
            sh = sorted(r["shift"] for r in rs)
            print(f"  {grp:32s} {len(rs):5d}  mean difference median {md[len(md) // 2]:.2f} / 255, worst "
                  f"{md[-1]:.2f}; edge offset median {sh[len(sh) // 2]:.3f} px, 99% {sh[int(len(sh) * .99)]:.3f}, "
                  f"worst {sh[-1]:.3f}")
        worst = sorted(rows, key=lambda r: -r["shift"])[:10]
        print("  worst edge offsets: " + ", ".join(f"{chr(r['char'])} {r['shift']:.3f}" for r in worst))
        specimen(work, style, info, index, t8, rows)
    with open(os.path.join(work, "check.json"), "w", encoding="utf-8") as f:
        json.dump(report, f)
    return report


def specimen(work, style, info, index, t8, rows):
    """the specimen sheet: the font set from the .ttf by FreeType, then letters side by side with the game's own trace
    at the same size (8x: the game's picture as part B draws it at a drawing scale of 8). It shows the game's letters,
    so it goes to the work folder (a scratch folder), never the repo."""
    from PIL import Image, ImageDraw, ImageFont
    st = STYLES[style]
    cell = st["us"][1]
    path = info["path"]
    # labels: Windows' Yu Gothic (Latin and kana / kanji, so a label can name its letter), else the font itself
    label = ImageFont.truetype("C:/Windows/Fonts/YuGothM.ttc" if os.path.exists("C:/Windows/Fonts/YuGothM.ttc")
                               else path, 22)
    have = {int(c) for c in info["chars"]}
    have_s = lambda s: "".join(ch for ch in s if ord(ch) in have or ch == " ")  # noqa: E731
    W = 2000
    lines = [
        ("Latin", have_s("ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz"), 2),
        ("Digits and punctuation", have_s("0123456789 !\"#%&'()*+,-./:;<=>?@[]^_`{|}~"), 2),
        ("Accented", have_s("ÀÁÂÄÇÈÉÊËÌÍÎÏÑÒÓÔÖÙÚÛÜß àáâäçèéêëìíîïñòóôöùúûü ŒœªºÀ¡¿«»×"), 2),
        ("The game's symbols", have_s("$ (Pokédollar)  \\ (yen)  ♂ ♀ ★ ♪ … ‘’ “” ← ↑ → ↓ é α β ‰ ‱ □"), 2),
        ("Hiragana", have_s("あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをん"), 2),
        ("Katakana", have_s("アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲン"), 2),
        ("Kanji", have_s("一二三四五六七八九十百千万円年月日時分人大小中上下左右東西南北山川田町村森林空雨"
                         "花草石金銀火水木土風雲電気天地海島道店町駅"), 2),
        ("At the game's size (1x)", have_s("Welcome to the Orre region! ポケモン センター"), 1),
        ("Sample, 3x", have_s("It's the Pokédollar: $1,200!  こんにちは、オーレ地方へようこそ！"), 3),
    ]
    sheet = Image.new("L", (W, 2400), 255)
    dr = ImageDraw.Draw(sheet)
    y = 20
    dr.text((20, y), f"{st['family']} — specimen ({info['glyphs']} glyphs, {info['bytes'] // 1024} KB)", font=label,
            fill=0)
    y += 50
    for title, text, scale in lines:
        f = ImageFont.truetype(path, size=cell * scale)
        dr.text((20, y), title, font=label, fill=110)
        y += 30
        # wrap by width
        cur = ""
        for ch in text:
            if dr.textlength(cur + ch, font=f) > W - 40:
                dr.text((20, y), cur, font=f, fill=0)
                y += int(cell * scale * 1.5)
                cur = ""
            cur += ch
        if cur:
            dr.text((20, y), cur, font=f, fill=0)
            y += int(cell * scale * 1.5)
        y += 10
    sheet = sheet.crop((0, 0, W, y + 20))
    out1 = os.path.join(work, f"specimen_{style}_set.png")
    sheet.save(out1)
    # the side by side: game trace | font | overlay (red: only the game's, blue: only the font's)
    picks = [c for c in map(ord, "AaBbEegHMOQRSWxy0123&@$éß!?あぬゆアシツ漢字機撃最事語書") if c in have]
    font8 = ImageFont.truetype(path, size=cell * 8, layout_engine=ImageFont.Layout.BASIC)
    byc = {int(c): int(gi) for c, gi in info["chars"].items()}
    shift = {r["char"]: r["shift"] for r in rows}
    tiles = []
    for c in picks:
        gi = byc[c]
        g = index[gi]
        w, h, data = t8[gi]
        game = Image.frombytes("L", (w * 8, h * 8), data)
        M = 4
        im = Image.new("L", ((w + 2 * M) * 8, (cell + 2 * M + 8) * 8), 0)
        ImageDraw.Draw(im).text((M * 8, (M + info["baseline"]) * 8), chr(c), font=font8, fill=255, anchor="ls")
        ours = im.crop((M * 8, (M + g["yoff"]) * 8, (M + w) * 8, (M + g["yoff"] + h) * 8))
        ov = Image.merge("RGB", (game, ours, ours))
        inv = lambda i: i.point(lambda v: 255 - v)  # noqa: E731
        tile = Image.new("RGB", (max(w * 8 * 3 + 40, 420), h * 8 + 40), (255, 255, 255))
        tile.paste(inv(game).convert("RGB"), (0, 0))
        tile.paste(inv(ours).convert("RGB"), (w * 8 + 20, 0))
        tile.paste(Image.eval(ov, lambda v: 255 - v), (2 * (w * 8 + 20), 0))
        ImageDraw.Draw(tile).text((0, h * 8 + 6), f"U+{c:04X} {chr(c)}  edge offset {shift.get(c, 0):.3f} px",
                                  font=ImageFont.truetype(label.path, 18), fill=(80, 80, 80))
        tiles.append(tile)
    cols = 3
    tw = max(t.width for t in tiles) + 30
    th = max(t.height for t in tiles) + 20
    rows_n = (len(tiles) + cols - 1) // cols
    comp = Image.new("RGB", (tw * cols + 40, th * rows_n + 90), (255, 255, 255))
    ImageDraw.Draw(comp).text((20, 15), "Each letter: the game's own trace (part B at 8x) | the font, drawn by FreeType "
                              "at 8x | both over each other (red: only the game's ink, cyan: only the font's)",
                              font=label, fill=(0, 0, 0))
    for i, t in enumerate(tiles):
        comp.paste(t, (20 + (i % cols) * tw, 60 + (i // cols) * th))
    out2 = os.path.join(work, f"specimen_{style}_compare.png")
    comp.save(out2)
    print(f"  specimen: {out1}\n            {out2}")


# --- the command line ---------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["build", "check"])
    ap.add_argument("--us", help="the US disc image (GC6E01)")
    ap.add_argument("--jp", help="the Japanese disc image (GC6J01)")
    ap.add_argument("--work", required=True, help="a scratch folder outside the repo (it holds the disc's letters)")
    ap.add_argument("--style", choices=["regular", "small", "both"], default="regular")
    ap.add_argument("--dest", default=os.path.join(REPO, "port", "ui", "traced", "fonts"))
    ap.add_argument("--keep-rare", action="store_true",
                    help="keep the kanji the game draws from Noto (as the game's own picture traced)")
    ap.add_argument("--crop", action="store_true", help="cut each letter at its glyph box, as the game's quad does")
    ap.add_argument("--jobs", type=int, default=12)
    ap.add_argument("--no-check", action="store_true")
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")  # letters in the report (python -I ignores PYTHONIOENCODING)
    work = os.path.abspath(a.work)
    if os.path.commonpath([work, REPO]) == REPO:
        raise SystemExit("--work must be outside the repo: it holds the disc's own letters")
    os.makedirs(work, exist_ok=True)
    styles = ["regular", "small"] if a.style == "both" else [a.style]
    if a.command == "build":
        if os.path.exists(os.path.join(work, "glyphs.json")):
            print("1. the glyph dump: reusing " + os.path.join(work, "glyphs.json"))
            index = load_index(work)
        else:
            if not a.us or not a.jp:
                raise SystemExit("build needs --us and --jp (the disc images) the first time")
            print("1. the glyph dump")
            index = dump(a.us, a.jp, work)
        built = {}
        bj = os.path.join(work, "build.json")
        if os.path.exists(bj):
            with open(bj, encoding="utf-8") as f:
                built = json.load(f)
        for style in styles:
            chars = pick(index, style)
            print(f"2. {STYLES[style]['family']}: {len(chars)} characters")
            print("3. the trace and the outlines")
            outlines = trace(work, style, chars, a.jobs, a.crop)
            print("4. the font")
            stats = build_font(style, index, chars, outlines, a.dest, a.keep_rare)
            stats["chars"] = {str(c): gi for c, gi in chars.items()
                              if not (outlines[gi]["noto"] and not a.keep_rare)}
            built[style] = stats
            print(f"  {stats['path']}: {stats['glyphs']} glyphs, {stats['bytes'] // 1024} KB; em {stats['upm']} units "
                  f"(the {STYLES[style]['us'][1]}-px cell), baseline {stats['baseline']} px below the line's top")
            print("  by group: " + ", ".join(f"{k} {v}" for k, v in sorted(stats["groups"].items())))
            print(f"  rare kanji the game draws from Noto, left out: {len(stats['rare_left_out'])}; "
                  f"largest share of a letter's traced ink outside its glyph box: {stats['spill_max']:.4f}; "
                  f"letters whose whole shape differs from the game's picture: {len(stats['not_kept'])}")
            with open(bj, "w", encoding="utf-8") as f:
                json.dump(built, f)
        if a.no_check:
            return
    print("5. the checks")
    check(work, styles)


if __name__ == "__main__":
    main()
