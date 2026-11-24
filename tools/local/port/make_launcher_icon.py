r"""Make the launchers' Windows icon (.ico) from OrrePort's own O SVGs (Phase 4 step 4.1, 2026-11-21).

    python -I tools/local/port/make_launcher_icon.py --colour rust|sage [--out port/launcher/orreport-<colour>.ico]
                                                     [--png-dir <folder>] [--check]

Why: OrrePort.exe and "OrrePort Studio.exe" (the portable folder's launchers, port/launcher/) showed Windows' generic
program icon in Explorer, because OrrePort's icon existed only as SVG pictures drawn at run time (pc_window_icon.cpp
for the game's window, studio/win32/theme.cpp for Studio's). Explorer can't run that code: it reads the icon from the
exe's resources, so the exe needs a real .ico compiled in (orre_launcher.rc's ICON line). snatcherLeo, 2026-11-21: "should
these 2 app icons have... icons from the app? We could use the dark mode ones".

What it draws: the same pictures, with the same rule, as the game's window icon (port/src/pc_window_icon.cpp,
apply_icons): the small O glyph (port/ui/icons/orreport-icon-small-<colour>-glyph.svg) below 48 px, the full icon on its
rounded backing (orreport-icon-<colour>.svg) from 48 px up. Sizes 16, 20, 24, 32, 40, 48, 64, 96, 128 and 256: 16 / 32
times Windows' display scalings (100-250%) plus Explorer's view sizes. Two colours are used (snatcherLeo, 2026-11-21: "OrrePort
Studio should use the sage O as in-app, but the OrrePort hub needs to use the dark red one that we have"):
- orreport-rust.ico for OrrePort.exe: the rust ("dark red") O, the art the game and hub window shows;
- orreport-sage.ico for "OrrePort Studio.exe": the sage O Studio's dark-mode window shows (theme.cpp recolours the rust
  files to sage's two colours; the result is identical to the sage files, checked with a diff 2026-11-21).

How it draws: a Python port of pc_window_icon.cpp's own reader and painter, so the .ico and the window icon match
(likely pixel for pixel; both use 4 x 4 samples per pixel at the same sample positions and the same evenodd rule; not
compared byte for byte). The SVG subset is the one that file reads: filled <path>s made of M / L / Z with absolute
coordinates (evenodd) and one <rect> with rx, on a 512 x 512 canvas; anything else stops the script, so a new icon
drawn with curves can't silently come out wrong. Standard library only: no SVG library is installed (new downloads need
snatcherLeo's OK), and the 256 px PNG is encoded here too (png_entry says why).

The .ico layout: 32-bit BGRA bitmaps (with the 1-bit mask Windows still expects) for 16-128 px, a PNG for 256 px (the
Vista+ convention: a 256 px bitmap would be 256 KB on its own). Written by hand here, not by Pillow's ICO writer, which
stores every size as PNG and draws all sizes from one picture (we need the glyph at small sizes). The output is
byte-for-byte reproducible; --check regenerates it in memory and compares with the committed file.

Platforms: the .ico is Windows-only. Linux needs PNGs at the hicolor sizes (16-256, drawn by this same painter: use
--png-dir) installed with a .desktop file; macOS needs an .icns (iconutil from an .iconset of PNGs) inside the .app
bundle. The painter and --png-dir are the same on every OS (likely).
"""
import argparse
import math
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
MAIN = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
ICONS = os.path.join(MAIN, "port", "ui", "icons")
SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
FULL_FROM = 48  # pc_window_icon.cpp: "the plain O up to 40 px ... and the full icon, on its rounded backing, from 48 px"
SS = 4          # samples per pixel per axis, as pc_window_icon.cpp's draw_rows
CANVAS = 512.0


def parse(svg, name):
    """The filled shapes of one icon SVG in drawing order: (rgb, rect or None, rings); pc_window_icon.cpp's parse()."""
    svg = re.sub(r"<metadata>.*?</metadata>", "", svg, flags=re.S)  # the files' provenance block, not drawing
    if 'viewBox="0 0 512 512"' not in svg:
        sys.exit(f"{name}: not a 512 x 512 canvas; this painter reads only OrrePort's icon subset")
    shapes = []
    for tag in re.findall(r"<(?:rect|path|circle|ellipse|polygon|polyline|line|g|use|text|image)\b[^>]*>", svg):
        kind = tag[1:].split()[0]
        if kind not in ("rect", "path"):
            sys.exit(f"{name}: <{kind}> is outside the subset this painter reads (paths with M/L/Z, one rounded rect)")
        attrs = dict(re.findall(r'\s([\w:-]+)="([^"]*)"', tag))
        fill = attrs.get("fill", "")
        if not re.fullmatch(r"#[0-9A-Fa-f]{6}", fill):
            sys.exit(f"{name}: a <{kind}> without a plain #RRGGBB fill")
        rgb = tuple(int(fill[i:i + 2], 16) for i in (1, 3, 5))
        if kind == "rect":
            rect = (float(attrs["width"]), float(attrs["height"]), float(attrs.get("rx", "0")))
            shapes.append((rgb, rect, []))
            continue
        if attrs.get("fill-rule") != "evenodd":
            sys.exit(f"{name}: a path without fill-rule evenodd")
        d = attrs["d"]
        if re.search(r"[A-Za-z]", re.sub(r"[MLZ]", "", d)):
            sys.exit(f"{name}: a path command other than M / L / Z")
        rings = []
        for cmd, args in re.findall(r"([MLZ])([^MLZ]*)", d):
            if cmd == "M":
                rings.append([])
            if cmd in "ML":
                x, y = (float(v) for v in args.split())
                rings[-1].append((x, y))
        shapes.append((rgb, None, rings))
    if not shapes:
        sys.exit(f"{name}: no shapes")
    return shapes


def coverage(shape, size):
    """Per pixel, how many of its SS x SS samples fall inside the shape (pc_window_icon.cpp's inside(), done a sample
    row at a time: the crossings of the row with every edge give the inside spans, the same evenodd test)."""
    _, rect, rings = shape
    scale = CANVAS / size
    cols = size * SS
    hits = [0] * (size * size)
    edges = [(r[i], r[i - 1]) for r in rings for i in range(len(r))]
    for srow in range(size * SS):
        y = (srow + 0.5) / SS * scale
        spans = []
        if rect is not None:
            w, h, rx = rect
            if 0 <= y <= h:
                cy = min(max(y, rx), h - rx)
                dy = y - cy
                if dy * dy <= rx * rx:
                    half = math.sqrt(rx * rx - dy * dy)
                    # inside when x in [0, w] and its distance to the corner centre <= rx (inclusive, like inside())
                    spans.append((max(0.0, rx - half), min(w, w - rx + half), True))
        else:
            xs = []
            for (xi, yi), (xj, yj) in edges:
                if (yi > y) != (yj > y):
                    xs.append((xj - xi) * (y - yi) / (yj - yi) + xi)
            xs.sort()
            # a sample at x is inside when an odd number of crossings lie to its right (x < crossing), i.e. when it
            # sits in [xs[0], xs[1]), [xs[2], xs[3]), ...
            spans.extend((xs[k], xs[k + 1], False) for k in range(0, len(xs) - 1, 2))
        base = (srow // SS) * size
        for a, b, closed in spans:
            # sample column j sits at x = (j + 0.5) / SS * scale
            j0 = max(0, math.ceil(a / scale * SS - 0.5))
            if closed:
                j1 = min(cols - 1, math.floor(b / scale * SS - 0.5))
            else:
                j1 = min(cols - 1, math.ceil(b / scale * SS - 0.5) - 1)
            for j in range(j0, j1 + 1):
                hits[base + j // SS] += 1
    return hits


def draw(shapes, size):
    """The shapes painted in order onto a size x size straight-alpha RGBA picture (pc_window_icon.cpp's draw_rows)."""
    n = SS * SS
    acc = [[0.0, 0.0, 0.0, 0.0] for _ in range(size * size)]  # premultiplied r, g, b, a
    for shape in shapes:
        (sr, sg, sb), _, _ = shape
        sr, sg, sb = sr / 255.0, sg / 255.0, sb / 255.0
        for i, h in enumerate(coverage(shape, size)):
            if h:
                cov = h / n
                p = acc[i]
                p[0] = sr * cov + p[0] * (1 - cov)
                p[1] = sg * cov + p[1] * (1 - cov)
                p[2] = sb * cov + p[2] * (1 - cov)
                p[3] = cov + p[3] * (1 - cov)
    out = bytearray(size * size * 4)
    lround = lambda v: int(v + 0.5)  # the project's lround for these non-negative values (Python's round() rounds half to even)
    for i, (r, g, b, a) in enumerate(acc):
        if a > 0:
            out[i * 4:i * 4 + 4] = bytes((lround(r / a * 255), lround(g / a * 255), lround(b / a * 255), lround(a * 255)))
    return bytes(out)


def bmp_entry(rgba, size):
    """One .ico bitmap entry: BITMAPINFOHEADER (height doubled for the mask), bottom-up BGRA rows, the 1-bit AND mask
    (1 = transparent; Windows ignores it when the alpha channel is used, but older readers want it there)."""
    rows = []
    for y in range(size - 1, -1, -1):
        row = rgba[y * size * 4:(y + 1) * size * 4]
        rows.append(bytes(c for px in range(size) for c in (row[px * 4 + 2], row[px * 4 + 1], row[px * 4], row[px * 4 + 3])))
    mask_stride = ((size + 31) // 32) * 4
    mask = bytearray()
    for y in range(size - 1, -1, -1):
        bits = bytearray(mask_stride)
        for x in range(size):
            if rgba[(y * size + x) * 4 + 3] == 0:
                bits[x // 8] |= 0x80 >> (x % 8)
        mask += bits
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, len(rows) * size * 4 + len(mask), 0, 0, 0, 0)
    return header + b"".join(rows) + bytes(mask)


def png_entry(rgba, size):
    """A plain RGBA PNG (8 bits per channel, no row filters, zlib level 9), with the standard library only: Pillow is
    installed for the user but not visible under python -I, and a hand-written encoder keeps the bytes reproducible
    whatever Pillow version a PC has."""
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + rgba[y * size * 4:(y + 1) * size * 4] for y in range(size))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def build(colour, png_dir=None):
    full = parse(open(os.path.join(ICONS, f"orreport-icon-{colour}.svg"), encoding="utf-8").read(), f"orreport-icon-{colour}.svg")
    small_name = f"orreport-icon-small-{colour}-glyph.svg"
    small = parse(open(os.path.join(ICONS, small_name), encoding="utf-8").read(), small_name)
    entries = []
    for size in SIZES:
        rgba = draw(full if size >= FULL_FROM else small, size)
        if png_dir:
            os.makedirs(png_dir, exist_ok=True)
            with open(os.path.join(png_dir, f"orreport-{colour}-{size}.png"), "wb") as f:
                f.write(png_entry(rgba, size))
        entries.append((size, png_entry(rgba, size) if size >= 256 else bmp_entry(rgba, size)))
    # ICONDIR, then one ICONDIRENTRY per picture (width / height 0 means 256), then the pictures
    out = struct.pack("<HHH", 0, 1, len(entries))
    offset = 6 + 16 * len(entries)
    for size, data in entries:
        dim = 0 if size >= 256 else size
        out += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    return out + b"".join(data for _, data in entries)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--colour", required=True, choices=("rust", "sage", "white"),
                    help="rust: OrrePort.exe's icon; sage: OrrePort Studio.exe's")
    ap.add_argument("--out", help="default: port/launcher/orreport-<colour>.ico")
    ap.add_argument("--png-dir", help="also write each size as a PNG here (previews; the Linux hicolor set)")
    ap.add_argument("--check", action="store_true", help="regenerate in memory and compare with --out; write nothing")
    a = ap.parse_args()
    out = a.out or os.path.join(MAIN, "port", "launcher", f"orreport-{a.colour}.ico")
    data = build(a.colour, a.png_dir)
    if a.check:
        same = os.path.isfile(out) and open(out, "rb").read() == data
        print(("same: " if same else "DIFFERENT: ") + out)
        sys.exit(0 if same else 1)
    with open(out, "wb") as f:
        f.write(data)
    print(f"wrote {out} ({len(data)} bytes, sizes {', '.join(map(str, SIZES))})")


if __name__ == "__main__":
    main()
