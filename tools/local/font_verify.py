"""Pixel-exact check of the game's main-font Latin letters against MS PGothic, several renderers (snatcherLeo 2026-10-18:
"verify MS PGothic carefully before we rely on it").

    python tools/local/font_verify.py <00_common_rel.rel> <out folder> [--font C:/Windows/Fonts/msgothic.ttc --face 2]

For every ASCII character of the main font (font 0, 22 px; colo/font.py) it renders the character with:
- gdi_gray4: Windows GDI GetGlyphOutlineW(GGO_GRAY4_BITMAP), 17 levels 0-16, em height 22 (lfHeight = -22);
- gdi_mono: GetGlyphOutlineW(GGO_BITMAP), 1 bit, hinted outline;
- ft_aa: FreeType through Pillow, anti-aliased (8-bit), 22 px;
- ft_mono: FreeType through Pillow, fontmode "1" (no smoothing; FreeType uses the font's embedded 1-bit 22 px strike
  when there is one).
Each render is reduced to the game's 16 levels (0-15) the plain way for its kind (gray4: v * 15 / 16 rounded;
8-bit: v * 15 / 255 rounded; 1-bit: 0 or 15) and placed over the game's glyph at the offset with the most equal
pixels (search +-6 px). Reports per method: characters identical pixel for pixel, and the mean number of differing
pixels; per character: the differing pixels for each method. Writes side-by-side PNGs (8x; game | render | difference)
for the closest and the worst characters to <out>. Nothing is downloaded; it reads installed fonts only.
"""
import argparse, ctypes, ctypes.wintypes as wt, os, sys
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from colo.font import parse_fonts  # noqa: E402

SEC4, MAIN_FONT = 0x184C, 0x2D2C


# ---- GDI ----
class MAT2(ctypes.Structure):
    _fields_ = [("eM11", ctypes.c_int32), ("eM12", ctypes.c_int32), ("eM21", ctypes.c_int32), ("eM22", ctypes.c_int32)]


class GLYPHMETRICS(ctypes.Structure):
    _fields_ = [("gmBlackBoxX", wt.UINT), ("gmBlackBoxY", wt.UINT), ("gmptGlyphOrigin", wt.POINT),
                ("gmCellIncX", ctypes.c_short), ("gmCellIncY", ctypes.c_short)]


GGO_BITMAP, GGO_GRAY4_BITMAP = 1, 5
gdi32, user32 = ctypes.windll.gdi32, ctypes.windll.user32
gdi32.CreateFontW.restype = wt.HFONT
gdi32.CreateFontW.argtypes = [ctypes.c_int] * 5 + [wt.DWORD] * 8 + [wt.LPCWSTR]
gdi32.CreateCompatibleDC.restype = wt.HDC
gdi32.CreateCompatibleDC.argtypes = [wt.HDC]
gdi32.SelectObject.restype = wt.HGDIOBJ
gdi32.SelectObject.argtypes = [wt.HDC, wt.HGDIOBJ]
gdi32.GetGlyphOutlineW.argtypes = [wt.HDC, wt.UINT, wt.UINT, ctypes.POINTER(GLYPHMETRICS), wt.DWORD, ctypes.c_void_p,
                                   ctypes.POINTER(MAT2)]
gdi32.GetGlyphOutlineW.restype = wt.DWORD


class Gdi:
    def __init__(self, face_name, height, charset=128):
        self.dc = gdi32.CreateCompatibleDC(None)
        # CreateFontW(height, width, esc, orient, weight, italic, underline, strike, charset, outprec, clipprec,
        #             quality, pitch, face); charset 128 = SHIFTJIS, 1 = DEFAULT (for fonts without Japanese:
        # asking them for SHIFTJIS makes Windows substitute a Japanese font, verified 2026-10-18)
        self.font = gdi32.CreateFontW(height, 0, 0, 0, 400, 0, 0, 0, charset, 0, 0, 0, 0, face_name)
        gdi32.SelectObject(self.dc, self.font)
        self.mat = MAT2(1 << 16, 0, 0, 1 << 16)
        self.levels = lambda v, top: round(v * 15 / top)   # how a converter maps GDI's levels to 4 bits (varied)

    def glyph(self, ch, fmt):
        gm = GLYPHMETRICS()
        size = gdi32.GetGlyphOutlineW(self.dc, ord(ch), fmt, ctypes.byref(gm), 0, None, ctypes.byref(self.mat))
        if size in (0, 0xFFFFFFFF):
            return [], gm
        buf = (ctypes.c_ubyte * size)()
        gdi32.GetGlyphOutlineW(self.dc, ord(ch), fmt, ctypes.byref(gm), size, buf, ctypes.byref(self.mat))
        w, h = gm.gmBlackBoxX, gm.gmBlackBoxY
        rows = []
        if fmt in (4, 5, 6):   # GGO_GRAY2 (0-4), GRAY4 (0-16), GRAY8 (0-64) bitmaps, byte per pixel, DWORD rows
            pitch = (w + 3) & ~3
            top = {4: 4, 5: 16, 6: 64}[fmt]
            for y in range(h):
                rows.append([self.levels(min(buf[y * pitch + x], top), top) for x in range(w)])
        else:
            pitch = ((w + 31) // 32) * 4
            for y in range(h):
                rows.append([15 if buf[y * pitch + x // 8] & (0x80 >> (x % 8)) else 0 for x in range(w)])
        return rows, gm


def ft_render(font, ch, mono):
    im = Image.new("L", (48, 48), 0)
    d = ImageDraw.Draw(im)
    if mono:
        d.fontmode = "1"
    d.text((8, 8), ch, font=font, fill=255)
    bbox = im.getbbox()
    if not bbox:
        return []
    im = im.crop(bbox)
    px = im.load()
    return [[round(px[x, y] * 15 / 255) for x in range(im.width)] for y in range(im.height)]


def compare(game, cand, search=6):
    """Place cand over the game glyph (both as rows of 0-15); return (differing pixels, total pixels, best ox, oy),
    counting over the union of both bitmaps at the best offset."""
    gh, gw = len(game), len(game[0]) if game else 0
    ch, cw = len(cand), len(cand[0]) if cand else 0
    if not gh or not ch:
        return None
    gpos = {(x, y): v for y, r in enumerate(game) for x, v in enumerate(r) if v}
    best = None
    for oy in range(-search, gh - ch + search + 1):
        for ox in range(-search, gw - cw + search + 1):
            cpos = {(x + ox, y + oy): v for y, r in enumerate(cand) for x, v in enumerate(r) if v}
            keys = set(gpos) | set(cpos)
            diff = sum(1 for k in keys if gpos.get(k, 0) != cpos.get(k, 0))
            if best is None or diff < best[0]:
                best = (diff, len(keys), ox, oy)
    return best


def side_by_side(game, cand, res, path, scale=8):
    diff, total, ox, oy = res
    W = max(len(game[0]), len(cand[0]) + max(ox, 0)) + 2 + max(-ox, 0)
    H = max(len(game), len(cand) + max(oy, 0)) + 2 + max(-oy, 0)
    gx, gy = 1 + max(-ox, 0), 1 + max(-oy, 0)
    img = Image.new("RGB", (W * 3 * scale + 2 * scale, H * scale), (30, 30, 36))
    p = img.load()

    def put(px0, x, y, col):
        for yy in range(scale):
            for xx in range(scale):
                p[px0 + x * scale + xx, y * scale + yy] = col
    gpos = {(x + gx, y + gy): v for y, r in enumerate(game) for x, v in enumerate(r)}
    cpos = {(x + gx + ox, y + gy + oy): v for y, r in enumerate(cand) for x, v in enumerate(r)}
    for (x, y), v in gpos.items():
        if v:
            put(0, x, y, (v * 17,) * 3)
    for (x, y), v in cpos.items():
        if v and 0 <= x < W and 0 <= y < H:
            put((W + 1) * scale, x, y, (v * 17,) * 3)
    for k in set(gpos) | set(cpos):
        a, b = gpos.get(k, 0), cpos.get(k, 0)
        x, y = k
        if not (0 <= x < W and 0 <= y < H):
            continue
        if a == b and a:
            put((2 * W + 2) * scale, x, y, (a * 17,) * 3)
        elif a != b:
            # red: only / brighter in the game; cyan: only / brighter in the render
            put((2 * W + 2) * scale, x, y, (255, 60, 60) if a > b else (60, 220, 255))
    img.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rel")
    ap.add_argument("out")
    ap.add_argument("--font", default="C:/Windows/Fonts/msgothic.ttc")
    ap.add_argument("--face", type=int, default=2)
    ap.add_argument("--gdi-name", default="MS PGothic")
    ap.add_argument("--px", type=int, default=22)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    rel = open(a.rel, "rb").read()
    glyphs = {g["char"]: g for g in parse_fonts(rel[SEC4 + MAIN_FONT:])[0]["glyphs"]}
    chars = [c for c in sorted(glyphs) if 0x21 <= c <= 0x7E]
    gdi = Gdi(a.gdi_name, -a.px)
    ftf = ImageFont.truetype(a.font, a.px, index=a.face)
    import math
    gdi_clip = Gdi(a.gdi_name, -a.px)
    gdi_clip.levels = lambda v, top: min(v, 15) if top == 16 else round(v * 15 / top)   # 0-16 -> 0-15 by clipping
    gdi_ceil = Gdi(a.gdi_name, -a.px)
    gdi_ceil.levels = lambda v, top: math.ceil(v * 15 / top)
    methods = {
        "gdi_gray4": lambda ch: gdi.glyph(ch, GGO_GRAY4_BITMAP)[0],
        "gdi_gray4_clip": lambda ch: gdi_clip.glyph(ch, GGO_GRAY4_BITMAP)[0],
        "gdi_gray8": lambda ch: gdi.glyph(ch, 6)[0],
        "gdi_gray8_ceil": lambda ch: gdi_ceil.glyph(ch, 6)[0],
        "gdi_gray2": lambda ch: gdi.glyph(ch, 4)[0],
        "gdi_mono": lambda ch: gdi.glyph(ch, GGO_BITMAP)[0],
        "ft_aa": lambda ch: ft_render(ftf, ch, False),
        "ft_mono": lambda ch: ft_render(ftf, ch, True),
    }
    table = {}
    for c in chars:
        ch = chr(c)
        game = [r[:] for r in glyphs[c]["pixels"]]
        # trim the game glyph's empty rows / columns (its cell includes the spacing pixel and the blank rows)
        ys = [y for y, r in enumerate(game) if any(r)]
        xs = [x for r in game for x, v in enumerate(r) if v]
        if not ys:
            continue
        game = [r[min(xs):max(xs) + 1] for r in game[min(ys):max(ys) + 1]]
        table[ch] = {"game": game}
        for m, f in methods.items():
            cand = f(ch)
            res = compare(game, cand) if cand else None
            table[ch][m] = (cand, res)
    print(f"{len(table)} ASCII glyphs; font {a.font} face {a.face} (GDI name '{a.gdi_name}'), {a.px} px")
    for m in methods:
        rs = [t[m][1] for t in table.values() if t[m][1]]
        exact = sum(1 for r in rs if r[0] == 0)
        # at each character's best offset: shape-only (both thresholded at level 8) and level tolerance
        shape_same = tol1 = tol3 = pix = 0
        ink_g = ink_c = 0
        shape_exact = 0
        for t in table.values():
            if not t[m][1]:
                continue
            cand, (_, _, ox, oy) = t[m][0], t[m][1]
            gpos = {(x, y): v for y, r in enumerate(t["game"]) for x, v in enumerate(r) if v}
            cpos = {(x + ox, y + oy): v for y, r in enumerate(cand) for x, v in enumerate(r) if v}
            keys = set(gpos) | set(cpos)
            bad = 0
            for k in keys:
                g, c = gpos.get(k, 0), cpos.get(k, 0)
                pix += 1
                shape_same += (g >= 8) == (c >= 8)
                bad += (g >= 8) != (c >= 8)
                tol1 += abs(g - c) <= 1
                tol3 += abs(g - c) <= 3
            shape_exact += bad == 0
            ink_g += sum(gpos.values())
            ink_c += sum(cpos.values())
        print(f"{m:10s} identical {exact}/{len(rs)}; mean differing pixels {sum(r[0] for r in rs) / len(rs):.1f} "
              f"of {sum(r[1] for r in rs) / len(rs):.0f}; pixels within 1 level {tol1 / pix:.0%}, within 3 {tol3 / pix:.0%}; "
              f"shape (level >= 8) agrees {shape_same / pix:.0%}, chars with identical shape {shape_exact}/{len(rs)}; "
              f"ink game / render {ink_g / max(ink_c, 1):.2f}")
    print("per character, differing pixels: gdi_gray4 / gdi_mono / ft_aa / ft_mono")
    line = []
    for ch, t in table.items():
        line.append(f"{ch} " + "/".join(str(t[m][1][0]) if t[m][1] else "-" for m in methods))
    print("  " + ",  ".join(line))
    best_m = min(methods, key=lambda m: sum(t[m][1][0] for t in table.values() if t[m][1]))
    ranked = sorted(((t[best_m][1][0], ch) for ch, t in table.items() if t[best_m][1]))
    for diff, ch in ranked[:4] + ranked[-8:]:
        name = f"{best_m}_{ord(ch):04X}_{diff}px.png"
        side_by_side(table[ch]["game"], table[ch][best_m][0], table[ch][best_m][1], os.path.join(a.out, name))
    print(f"side-by-side images ({best_m}, 4 closest + 8 worst) in {a.out}")


if __name__ == "__main__":
    main()
