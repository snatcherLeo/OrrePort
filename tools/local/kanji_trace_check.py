"""Checks the tracer on every letter of the Japanese disc (region work piece 3; snatcherLeo, 2026-11-15: "For the kanji
tracing, we have to get as exact as we can, and it's fine if you take time with it, just like we did English", then
"Trace it, go slow and line up as best you can").

What it does: reads every glyph of the Japanese disc's fonts (the font files of resource type 6 in each archive, and
the main fonts inside its main.dol, found by their headers), one per font id and character, and runs part B's tracer
on each exactly as font_trace_mockup.traced() does (bicubic 16x, the settings tried smoothest first, the first that
keeps the glyph's structure and stays near the original wins). For each glyph it records:
  - pieces / holes: the game's picture's separate ink pieces and enclosed gaps, and the trace's (shrunk to 4x);
  - structure: how many pieces or holes the trace lost or added (0 = every stroke and gap kept);
  - round trip: the trace shrunk back to the game's size, its mean difference from the game's pixels (0-255 a pixel);
  - the setting chosen.
Then writes a CSV of all of them and contact sheets (the game's picture enlarged without smoothing beside the trace,
at 4x), worst first: structure changes, then the largest round trips. Output goes to a folder outside the repo (it
shows the game's letters, which never enter git).

    python tools/local/kanji_trace_check.py <Japanese disc image> <out folder> [--jobs 8] [--sheet 120]

Needs Pillow (plain python, not -I: Pillow is in the user's site-packages). Uses tools/local/colo (the disc, fsys and
font readers). Platforms: any OS with Python 3 and Pillow. XD: the same tool would read XD's Japanese disc once its
fonts' places are known (likely the same layout: the shared engine's GSmsg; a guess until XD is studied).
"""
import argparse
import csv
import os
import struct
import sys
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw, ImageFilter, ImageFont  # noqa: E402

from colo.disc import Disc  # noqa: E402
from colo.font import parse_fonts  # noqa: E402
from colo.fsys import archives, fsys_entries  # noqa: E402
from font_trace_mockup import ROUND_TRIP_SLACK, SETTINGS, UP, glyph_image, topology  # noqa: E402

S = 4  # the sheets' scale (a 1920-pixel-tall screen draws the 22-pixel font at about 4x)


def kind(c):
    if 0x4E00 <= c <= 0x9FFF or c == 0x3005:
        return "kanji"
    if 0x3040 <= c <= 0x309F:
        return "hiragana"
    if 0x30A0 <= c <= 0x30FF:
        return "katakana"
    return "other"


def dol_fonts(dol):
    """the fonts inside a main.dol, by their headers (as pc_region_text.cpp's add_fonts_found)"""
    out = []
    o = 0
    while o + 0x20 < len(dol):
        fid, cw, ch, size, count = struct.unpack(">HBBIH", dol[o:o + 10])
        doff = struct.unpack(">I", dol[o + 0x0C:o + 0x10])[0]
        ok = fid <= 4 and 8 <= cw <= 32 and 8 <= ch <= 32 and 10 <= count <= 8000 and doff >= 0x10 + 8 * count
        ok = ok and o + 0x18 + 8 * count <= len(dol) and (size == 0 or size >= 0x18 + 8 * count)
        if ok:
            codes = [struct.unpack(">H", dol[o + 0x18 + 8 * g:o + 0x1A + 8 * g])[0] for g in range(min(count, 60))]
            ok = all(b > a for a, b in zip(codes, codes[1:]))
        if ok:
            for f in parse_fonts(dol[o:]):
                out.append(f)
                break  # this font only (its chain is found by the scan too)
            o += 0x18 + 8 * count
            continue
        o += 4
    return out


def gather(iso):
    """every glyph, one per (font id, cell height, character); main.dol fonts first (the game's main letters)"""
    d = Disc(iso)
    fonts = dol_fonts(d.dol())
    for name, arc in archives(d):
        for k, ename, ftype, ident, load in fsys_entries(arc):
            if ftype == 6:
                fonts += parse_fonts(load())
    seen = {}
    for f in fonts:
        for g in f["glyphs"]:
            key = (f["id"], f["cell_h"], g["char"])
            if key not in seen and g["width"] > 0 and len(g["pixels"]) > 0:
                seen[key] = g
    return seen


def check(item):
    """the tracer's choice for one glyph, with its measures (font_trace_mockup.traced's choice, measured)"""
    (fid, ch, code), g = item
    im = glyph_image(g)
    pad = 2
    big = Image.new("L", (im.width + 2 * pad, im.height + 2 * pad), 0)
    big.paste(im, (pad, pad))
    up = big.resize((big.width * UP, big.height * UP), Image.BICUBIC)
    want = topology(big.point(lambda v: 255 if v >= 128 else 0), 1)
    src = list(big.getdata())
    tried = []
    for i, (blur, th) in enumerate(SETTINGS):
        base = up.filter(ImageFilter.GaussianBlur(UP * blur)) if blur else up
        cand = base.point(lambda v, th=th: 255 if v >= th else 0)
        got = topology(cand.resize((big.width * 4, big.height * 4), Image.BOX).point(lambda v: 255 if v >= 128 else 0), 4)
        dist = abs(got[0] - want[0]) + abs(got[1] - want[1])
        back = list(cand.resize(big.size, Image.BOX).getdata())
        err = sum(abs(a - b) for a, b in zip(src, back))
        tried.append((dist, err, i, got))
    closest = min(t[0] for t in tried)
    ok = [t for t in tried if t[0] == closest]
    least = min(t[1] for t in ok)
    dist, err, choice, got = next(t for t in ok if t[1] <= least * ROUND_TRIP_SLACK + 1)
    return {"font": fid, "cell": ch, "char": code, "kind": kind(code), "pieces": want[0], "holes": want[1],
            "trace_pieces": got[0], "trace_holes": got[1], "structure": dist,
            "round_trip": round(err / max(1, len(src)), 2), "setting": choice}


def trace_image(g, setting):
    """the trace at S for the sheets, with the setting the check chose"""
    blur, th = SETTINGS[setting]
    im = glyph_image(g)
    pad = 2
    big = Image.new("L", (im.width + 2 * pad, im.height + 2 * pad), 0)
    big.paste(im, (pad, pad))
    up = big.resize((big.width * UP, big.height * UP), Image.BICUBIC)
    base = up.filter(ImageFilter.GaussianBlur(UP * blur)) if blur else up
    out = base.point(lambda v: 255 if v >= th else 0).resize((big.width * S, big.height * S), Image.BOX)
    return out.crop((pad * S, pad * S, pad * S + im.width * S, pad * S + im.height * S))


def sheets(rows, glyphs, out, per):
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 14)
    except OSError:
        font = ImageFont.load_default()
    cols = 10
    for n in range(0, len(rows), per):
        part = rows[n:n + per]
        cell_w, cell_h = 2 * 22 * S + 26, 22 * S + 34
        img = Image.new("RGB", (cols * cell_w + 10, ((len(part) + cols - 1) // cols) * cell_h + 10), (17, 19, 19))
        d = ImageDraw.Draw(img)
        for i, r in enumerate(part):
            g = glyphs[(r["font"], r["cell"], r["char"])]
            x0, y0 = (i % cols) * cell_w + 10, (i // cols) * cell_h + 10
            orig = glyph_image(g).resize((g["width"] * S, len(g["pixels"]) * S), Image.NEAREST)
            img.paste(Image.merge("RGB", [orig] * 3), (x0, y0))
            img.paste(Image.merge("RGB", [trace_image(g, r["setting"])] * 3), (x0 + 22 * S + 6, y0))
            colour = (232, 149, 90) if r["structure"] else (150, 156, 153)
            d.text((x0, y0 + 22 * S + 2), "U+%04X f%d %dpx  s%d  rt%.1f" % (r["char"], r["font"], r["cell"], r["structure"],
                                                                         r["round_trip"]), font=font, fill=colour)
        img.save(os.path.join(out, "sheet_%03d.png" % (n // per + 1)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("iso")
    ap.add_argument("out")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--sheet", type=int, default=120)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    glyphs = gather(a.iso)
    print("glyphs:", len(glyphs))
    with Pool(a.jobs) as pool:
        rows = pool.map(check, list(glyphs.items()), chunksize=16)
    rows.sort(key=lambda r: (-r["structure"], -r["round_trip"]))
    with open(os.path.join(a.out, "trace_check.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    changed = [r for r in rows if r["structure"]]
    print("structure kept:", len(rows) - len(changed), " changed:", len(changed))
    for k in ("kanji", "hiragana", "katakana", "other"):
        sub = [r for r in rows if r["kind"] == k]
        if sub:
            print("  %-9s %5d  changed %4d  mean round trip %.2f" % (k, len(sub), sum(1 for r in sub if r["structure"]),
                                                                    sum(r["round_trip"] for r in sub) / len(sub)))
    sheets(rows, glyphs, a.out, a.sheet)
    print("sheets written to", a.out)


if __name__ == "__main__":
    main()
