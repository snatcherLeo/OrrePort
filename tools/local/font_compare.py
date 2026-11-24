"""Score installed fonts against the game's main font, Latin and kana separately (snatcherLeo's 2026-10-18 font task:
choose the open HD font by measurement, as the MS PGothic comparison in docs/engine/fonts.md was).

    python tools/local/font_compare.py <extracted/other/common/original/00_common_rel.rel>
    (not -I: Pillow is installed in the user site-packages, which -I hides)

Reads the main font (font 0, 22 px cells) from common_rel (section 4 + 0x2D2C, colo/font.py) and, for every
candidate font (installed files only; nothing is downloaded or copied), per character:
- advance: the font's advance width at 22 px (Pillow getlength, rounded) against the game's advance; reported as the
  share of exact matches and of "+1" matches (the game's converter likely added one pixel of spacing to MS PGothic's
  Latin), and the mean absolute difference;
- shape: the character rendered at 4x (88 px) and shrunk to 22 px, then the best Pearson correlation with the game's
  4-bit bitmap over all placements within +-4 px (ink only, so the vertical metrics don't matter).
Groups: Latin = ASCII letters and digits (A-Z a-z 0-9); kana = the game's hiragana and katakana glyphs.
Prints one row per font (means), plus the ten worst characters per group for the best font in each group.
MS PGothic / MS Gothic are controls (the known answer for Latin), not candidates: they can't be shipped.
"""
import argparse, os, sys
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from colo.font import parse_fonts  # noqa: E402

SEC4, MAIN_FONT = 0x184C, 0x2D2C
WIN = "C:/Windows/Fonts/"
# (label, file, face index); shippable = open licence (OFL / IPA / M+ / VL), control = for checking the method only
CANDIDATES = [
    # msgothic.ttc faces (checked with getname, 2026-10-18): 0 MS Gothic, 1 MS UI Gothic, 2 MS PGothic
    ("MS PGothic (control)", WIN + "msgothic.ttc", 2),
    ("MS Gothic (control)", WIN + "msgothic.ttc", 0),
    ("BIZ UDPGothic", WIN + "BIZ-UDGothicR.ttc", 1),
    ("BIZ UDGothic", WIN + "BIZ-UDGothicR.ttc", 0),
    ("BIZ UDPGothic Bold", WIN + "BIZ-UDGothicB.ttc", 1),
    ("VL PGothic", WIN + "VL-PGothic-Regular.ttf", 0),
    ("VL Gothic", WIN + "VL-Gothic-Regular.ttf", 0),
    ("Noto Sans JP (variable, default)", WIN + "NotoSansJP-VF.ttf", 0),
]


def gdi_widths(path, family, chars, px=22):
    """Advance widths as Windows lays text out (the font's hinted per-size widths, GetGlyphOutlineW's gmCellIncX at
    an em height of px). *Corrected 2026-10-18:* Pillow's getlength gives the unhinted outline width, about 1 px
    narrower for MS PGothic at 22 px, which made this tool's width column disagree with the earlier browser
    measurement; Windows' widths reproduce it (game = MS PGothic + 1 for 91 of 92 ASCII characters). A font file
    that isn't installed is loaded for this process only (AddFontResourceExW, FR_PRIVATE): nothing is installed."""
    import ctypes
    from font_verify import Gdi, GLYPHMETRICS, MAT2   # noqa: F401
    gdi32 = ctypes.windll.gdi32
    if not path.lower().startswith("c:/windows/fonts"):
        gdi32.AddFontResourceExW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_void_p]
        gdi32.AddFontResourceExW(path.replace("/", "\\"), 0x10, None)
    g = Gdi(family, -px, charset=1)
    # Windows silently substitutes another font when the name doesn't match (*verified* 2026-10-18: the Latin-only
    # fonts all came out with MS PGothic's widths); check the face it really selected, and give up on GDI widths
    # (None: the caller falls back to Pillow's unhinted widths and says so) if it isn't the one asked for.
    buf = ctypes.create_unicode_buffer(64)
    gdi32.GetTextFaceW.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_wchar_p]
    gdi32.GetTextFaceW(g.dc, 64, buf)
    if buf.value.lower() != family.lower():
        print(f"  (GDI selected '{buf.value}' for '{family}': widths from Pillow, unhinted)")
        return {}
    out = {}
    for c in chars:
        gm = GLYPHMETRICS()
        r = gdi32.GetGlyphOutlineW(g.dc, c, 0, ctypes.byref(gm), 0, None, ctypes.byref(g.mat))
        out[c] = gm.gmCellIncX if r != 0xFFFFFFFF else None
    return out


def game_glyphs(rel_path):
    rel = open(rel_path, "rb").read()
    font = parse_fonts(rel[SEC4 + MAIN_FONT:])[0]
    return {g["char"]: g for g in font["glyphs"]}


def groups(glyphs):
    latin = [c for c in glyphs if chr(c).isascii() and chr(c).isalnum()]
    kana = [c for c in glyphs if 0x3041 <= c <= 0x30FF and chr(c).isalpha()]
    return {"Latin": latin, "kana": kana}


def game_image(g):
    return [[v / 15.0 for v in row] for row in g["pixels"]]


STROKE = 0   # --stroke N: synthetic emboldening, an N-pixel outline at the 4x render = N / 4 px at 22 px


def render(font4, ch):
    """The character at 4x into a 128x128 canvas, shrunk to 32x32 (22 px nominal size)."""
    im = Image.new("L", (128, 128), 0)
    ImageDraw.Draw(im).text((8, 8), ch, font=font4, fill=255, stroke_width=STROKE, stroke_fill=255)
    return im.resize((32, 32), Image.LANCZOS)


def ink(img_rows):
    return sum(sum(r) for r in img_rows)


def best_corr(gimg, cand):
    """Best Pearson correlation of the game bitmap (h x w) placed anywhere over the 32x32 candidate render."""
    h, w = len(gimg), len(gimg[0]) if gimg else 0
    if not h or not w:
        return None
    g = [v for r in gimg for v in r]
    n = len(g)
    gm = sum(g) / n
    gd = [v - gm for v in g]
    gs = sum(d * d for d in gd) ** 0.5
    if gs == 0:
        return None
    px = cand.load()
    best = -1.0
    for oy in range(0, 32 - h + 1):
        for ox in range(0, 32 - w + 1):
            c = [px[ox + x, oy + y] / 255.0 for y in range(h) for x in range(w)]
            cm = sum(c) / n
            cd = [v - cm for v in c]
            cs = sum(d * d for d in cd) ** 0.5
            if cs == 0:
                continue
            r = sum(a * b for a, b in zip(gd, cd)) / (gs * cs)
            if r > best:
                best = r
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rel")
    ap.add_argument("--add", action="append", default=[],
                    help="an extra font file as 'label=path' (e.g. a downloaded open font); repeatable")
    ap.add_argument("--only-added", action="store_true", help="score only the --add fonts (plus the MS PGothic control)")
    ap.add_argument("--stroke", type=int, default=0, help="synthetic emboldening at the 4x render, in 4x pixels")
    ap.add_argument("--latin-only", action="store_true", help="score the Latin group only (faster)")
    ap.add_argument("--kana-only", action="store_true", help="score the kana group only (faster)")
    a = ap.parse_args()
    global CANDIDATES, STROKE
    STROKE = a.stroke
    extra = [(s.split("=", 1)[0], s.split("=", 1)[1], 0) for s in a.add]
    if a.only_added:
        CANDIDATES = CANDIDATES[:1] + extra
    else:
        CANDIDATES = CANDIDATES + extra
    glyphs = game_glyphs(a.rel)
    grp = groups(glyphs)
    print(f"main font: {len(glyphs)} glyphs; Latin {len(grp['Latin'])}, kana {len(grp['kana'])}")
    results = {}
    for label, path, idx in CANDIDATES:
        if not os.path.exists(path):
            print(f"{label}: not installed ({path})")
            continue
        f22 = ImageFont.truetype(path, 22, index=idx)
        f88 = ImageFont.truetype(path, 88, index=idx)
        if "variable" in label or "VF" in path or "[wght]" in path:   # a variable font: use Regular, not Thin
            for f in (f22, f88):
                try:
                    f.set_variation_by_name("Regular")
                except Exception:
                    pass
        name = " ".join(f22.getname())
        row = {"name": name}
        latin_only = "(Latin only)" in label
        widths = gdi_widths(path, f22.getname()[0], [c for cs in grp.values() for c in cs])
        for gname, chars in grp.items():
            if ((latin_only or a.latin_only) and gname == "kana") or (a.kana_only and gname == "Latin"):
                row[gname] = {"exact": 0, "plus1": 0, "mad": 0, "corr": float("nan"), "worst": []}
                continue
            exact = plus1 = 0
            diffs, corrs = [], {}
            for c in chars:
                g = glyphs[c]
                adv = widths.get(c)
                if adv is None:
                    adv = round(f22.getlength(chr(c)))
                exact += adv == g["width"]
                plus1 += adv + 1 == g["width"]
                diffs.append(abs(adv - g["width"]))
                r = best_corr(game_image(g), render(f88, chr(c)))
                if r is not None:
                    corrs[c] = r
            k = len(chars)
            row[gname] = {"exact": exact / k, "plus1": plus1 / k, "mad": sum(diffs) / k,
                          "corr": sum(corrs.values()) / max(len(corrs), 1), "worst": sorted(corrs.items(), key=lambda t: t[1])[:10]}
        results[label] = row
        L, K = row["Latin"], row["kana"]
        print(f"{label:34s} [{name}]  Latin: width exact {L['exact']:.0%} / +1 {L['plus1']:.0%} / mean |diff| "
              f"{L['mad']:.2f}, shape {L['corr']:.3f}   kana: width exact {K['exact']:.0%} / +1 {K['plus1']:.0%} / "
              f"mean |diff| {K['mad']:.2f}, shape {K['corr']:.3f}")
    for gname in ("Latin", "kana"):
        ship = {k: v for k, v in results.items() if "control" not in k and v[gname]["corr"] == v[gname]["corr"]}
        if not ship:
            continue
        best = max(ship, key=lambda k: ship[k][gname]["corr"])
        worst = ", ".join(f"{chr(c)} {r:.2f}" for c, r in ship[best][gname]["worst"])
        print(f"best shippable for {gname}: {best} (shape {ship[best][gname]['corr']:.3f}); its lowest: {worst}")


if __name__ == "__main__":
    main()
