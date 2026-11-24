"""Makes OrrePort's pictures for the PC screen's Main Menu row, once, as shipped files (2026-11-15).

Why it exists: snatcherLeo approved a fifth row on the game's PC screen, "Main Menu" (mockup docs/design/mockups/
pc-menu-main-menu.png), with its own icon on the PC screen's icon wheel. The wheel's icons are pictures with their
words drawn in (pcbox_menu menu_019: Pokémon / Save / Items / Exit, in the navy-and-white lettering of the wheel).
snatcherLeo first approved tracing the word at run time from the player's disc, then changed it the same day: "why this
"trace from your discs at run time", shouldn't we just.. include it with OrrePort? I'd prefer that", and "Just do what
makes the most sense and is easiest and will show up in our editors eventually". So this script makes, once:
  - the icon, drawn by code in the measured style of the wheel's icons (no disc data at all): an outlined rounded
    frame, two outlined cards on top (the main menu's two big mode buttons), then LOAD's small circle, the OPTIONS pill
    and Quit's circle (the same as LOAD's: snatcherLeo, 2026-11-15). Written to port/ui/game/pc_menu/main_menu_icon.png;
  - each language's wheel label ("Main Menu", "Hauptmenü", "Menu principal", "MENU PRINCIPALE", "Menú principal"),
    its letters TRACED from the discs' own wheel sheets into OrrePort's own smooth outlines (the method the font work
    uses, font_trace_mockup.traced: a 16x bicubic enlargement, a light blur, a cut at half coverage, area-averaged
    down), never the disc's pixels copied. Written to port/ui/traced/pc_menu/main_menu_label_<lang>.png. Japanese uses
    the English label (the Japanese disc's own wheel words are English: Pokémon / Report / Item / Back).
Both at 4x the game's size (600 x 680 for the 150 x 170 wheel piece; the label already in its place in that box), so a
UI editor can swap either file; the game draws them down to its size when the PC screen opens (pc_pc_screen.cpp).

Where each letter comes from (the approved mockup's choices, kept): most from the US sheet (a, i, n, e, t, m, S, v, E,
x); M from the Italian sheet's POKéMON; u, ü, r, c from the German sheet's Zurück; p from the Japanese disc's Report;
l from the Spanish Salir; H built from two stems of the US Items' I joined by the bar of the US Exit's E; ú the German
u with the acute accent of the US é. Italian: every capital from the Italian sheet itself (its wheel words are
capitals). Long labels are narrowed sideways to the wheel's room (140 game px), height and outline kept.

The traced labels are "traced letter shapes" in the project rules sense (snatcherLeo, 2026-11-14: "We ship the traces"): they go in
port/ui/traced/ only, each PNG carries the generated-from-disc marker in a text chunk, and the commit hook allows that
marker in that folder alone (tools/local/hooks/check_game_data.py). The icon has no disc data and no marker.

    python tools/local/pc_menu_pictures.py "<USA disc>" "<Europe disc>" "<Japan disc>" [--out <repo root>] [--preview <folder>]

Needs Pillow (user site-packages: run with plain python, not -I). Platforms: any OS with Python 3 and Pillow.
For XD: XD's PC screen has its own sheet (likely different places); the method (trace letters from the game's own
lettering, build missing ones from its strokes, ship the outlines) carries over (likely).
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageChops, ImageDraw, ImageFilter, PngImagePlugin  # noqa: E402

from colo.disc import Disc  # noqa: E402
from colo.fsys import fsys_entries  # noqa: E402
from colo.gx import decode_texture_file  # noqa: E402

S = 4                    # the shipped pictures' scale (4x the game's pixels)
UP = 16                  # the trace's working detail per game pixel (as font_trace_mockup.UP)
CELL = (150, 170)        # the wheel piece in game pixels: the icon's frame and the label below it
FRAME = (13, 19, 137, 143)
# *Changed 2026-11-15 (snatcherLeo: "make sure it's positioned properly, in comparison to the other boxes"; measured on the US
# sheet): the game's widest wheel words are 123 px with their outline (Items, Exit), so the letters are narrowed to 115
# (about 123 with the outline and the letters' own margins), not 140: "Main Menu" no longer reaches the next icon.
# Centred on the frame (75)
LABEL_BASELINE, LABEL_CENTRE, LABEL_MAXW = 161, 75, 115
MARKER = "ORREPORT-" + "GENERATED-FROM-DISC"  # built so this file itself doesn't hold the marker text

# the wheel's colours, measured on the sheets (navy fill, light outline) and on the icons (outline, blue rim, fill)
FILL = (0, 74, 139)
LINE = (213, 222, 238)
ICON_OUT, ICON_BLUE, ICON_FILL = (213, 222, 238, 255), (8, 115, 197, 255), (205, 222, 230, 255)

# the sheets: (disc key, archive name); each is pcbox_menu's menu_019, the wheel's icons with their words
SHEETS = {"us": ("us", "pcbox_menu"), "ge": ("eu", "pcbox_menu_ge"), "fr": ("eu", "pcbox_menu_fr"),
          "it": ("eu", "pcbox_menu_it"), "sp": ("eu", "pcbox_menu_sp"), "jp": ("jp", "pcbox_menu")}
SHEET_SIZE = (597, 170)
BAND = (134, 170)  # the rows holding the words

# letter -> (sheet, its navy fill's box x0, y0, x1, y1 on the sheet, that word's baseline); measured for the mockup
SRC = {
    "M": ("it", (80, 145, 97, 162), 162), "a": ("us", (207, 147, 219, 160), 160), "i": ("us", (518, 145, 522, 163), 163),
    "n": ("us", (104, 149, 114, 162), 162), "e": ("us", (237, 147, 250, 160), 160), "u": ("ge", (482, 152, 493, 164), 164),
    "ü": ("ge", (509, 146, 520, 164), 164), "p": ("jp", (212, 146, 224, 163), 159), "t": ("us", (336, 147, 344, 163), 163),
    "m": ("us", (363, 150, 380, 163), 163), "r": ("ge", (498, 151, 506, 164), 164), "c": ("ge", (524, 151, 536, 164), 164),
    "l": ("sp", (515, 147, 518, 164), 164), "S": ("us", (190, 142, 204, 160), 160), "v": ("us", (222, 147, 234, 160), 160),
    "E": ("us", (488, 146, 501, 163), 163), "x": ("us", (504, 150, 515, 163), 163),
}
# Italian: the wheel's Italian words are capitals (POKéMON, SALVA, STRUMENTI, ESCI), so MENU PRINCIPALE uses its sheet
SRC_IT = {
    "M": ("it", (80, 145, 97, 162), 162), "E": ("it", (482, 147, 494, 163), 163), "N": ("it", (120, 145, 134, 162), 162),
    "U": ("it", (347, 146, 358, 164), 163), "P": ("it", (14, 146, 27, 162), 162), "R": ("it", (332, 146, 344, 163), 163),
    "I": ("it", (535, 147, 538, 163), 163), "C": ("it", (516, 146, 531, 163), 163), "A": ("it", (197, 144, 213, 160), 160),
    "L": ("it", (217, 143, 228, 160), 160),
}
KERN = {"r": (0, -2), "t": (-1, -1)}  # optical spacing for the open letters (left, right), game px
LABELS = {"en": ("Main Menu", SRC), "de": ("Hauptmenü", SRC), "fr": ("Menu principal", SRC),
          "it": ("MENU PRINCIPALE", SRC_IT), "es": ("Menú principal", SRC)}


# ---------------------------------------------------------------- the discs' sheets (read, never written anywhere)
def read_sheets(discs):
    out = {}
    opened = {k: Disc(p) for k, p in discs.items()}
    for key, (disc, arc_name) in SHEETS.items():
        d = opened[disc]
        path = next((p for p in d.files if os.path.basename(p) == arc_name + ".fsys"), None)
        if path is None:
            sys.exit(f"{arc_name}.fsys not found on the {disc} disc")
        for _k, name, t, _ident, load in fsys_entries(d.file(path)):
            if t == 9 and name == "menu_019":
                w, h, rgba, _fmt = decode_texture_file(load())
                if (w, h) != SHEET_SIZE:
                    sys.exit(f"{arc_name} menu_019 is {w} x {h}, not the measured {SHEET_SIZE}: not the sheet measured")
                out[key] = Image.frombytes("RGBA", (w, h), bytes(rgba))
                break
        else:
            sys.exit(f"menu_019 not found in {arc_name}.fsys")
    return out


def fill_alpha(img):
    """how much of each pixel is the navy fill: 1 at the fill colour, 0 at the outline colour (linear in between,
    along the line between the two colours); 0 where transparent or off that line (the icons' blue)"""
    px = img.load()
    a = Image.new("L", img.size, 0)
    ap = a.load()
    d2 = sum((LINE[i] - FILL[i]) ** 2 for i in range(3))
    for y in range(BAND[0], BAND[1]):
        for x in range(img.width):
            p = px[x, y]
            if p[3] < 128:
                continue
            t = sum((LINE[i] - p[i]) * (LINE[i] - FILL[i]) for i in range(3)) / d2
            proj = [LINE[i] + (FILL[i] - LINE[i]) * t for i in range(3)]
            if sum((p[i] - proj[i]) ** 2 for i in range(3)) ** 0.5 > 40:
                continue
            ap[x, y] = max(0, min(255, round(t * 255)))
    return a


# ---------------------------------------------------------------- one letter: its coverage at the game's size
class Letters:
    def __init__(self, sheets, src):
        self.alpha = {k: fill_alpha(v) for k, v in sheets.items()}
        self.src = src

    def coverage(self, ch):
        """(fill coverage crop with a 1 px margin, the crop's top relative to the baseline), game pixels"""
        if ch in self.src:
            k, (x0, y0, x1, y1), bl = self.src[ch]
            return self.alpha[k].crop((x0 - 1, y0 - 1, x1 + 2, y1 + 2)), (y0 - 1) - bl
        if ch == "H":  # two stems of the US Items' I, joined by the middle bar of the US Exit's E
            stem = self.alpha["us"].crop((329 - 1, 147 - 1, 333 + 2, 163 + 2))
            soff = (147 - 1) - 163
            e, eoff = self.coverage("E")
            out = Image.new("L", (17, stem.height), 0)
            out.paste(stem, (0, 0))
            out.paste(ImageChops.lighter(out.crop((10, 0, 10 + stem.width, stem.height)), stem), (10, 0))
            ep = e.load()
            col = 1 + 8
            for y in [y for y in range(5, e.height - 5) if ep[col, y] > 0]:
                yy = y + (eoff - soff)
                for x in range(5, 12):
                    if 0 <= yy < out.height:
                        out.putpixel((x, yy), max(out.getpixel((x, yy)), ep[col, y]))
            return out, soff
        if ch == "ú":  # the German u with the acute accent of the US é (Pokémon)
            u, uoff = self.coverage("u")
            acc = self.alpha["us"].crop((62 - 1, 143 - 1, 73 + 2, 148))
            top = (143 - 1) - 162
            out = Image.new("L", (u.width, u.height - top + uoff), 0)
            out.paste(u, (0, -top + uoff))
            x = (u.width - acc.width) // 2 + 1
            out.paste(ImageChops.lighter(out.crop((x, 0, x + acc.width, acc.height)), acc), (x, 0))
            return out, top
        raise KeyError(ch)


def trace(cov):
    """a letter's coverage made into OrrePort's own smooth outline at S x: 16x bicubic, a 0.35 game-pixel blur, cut at
    half coverage, area-averaged down (font_trace_mockup.traced's method). Returns an L image S x the size"""
    pad = 2
    big = Image.new("L", (cov.width + 2 * pad, cov.height + 2 * pad), 0)
    big.paste(cov, (pad, pad))
    up = big.resize((big.width * UP, big.height * UP), Image.BICUBIC).filter(ImageFilter.GaussianBlur(0.35 * UP))
    cut = up.point(lambda v: 255 if v >= 128 else 0)
    small = cut.resize((big.width * S, big.height * S), Image.BOX)
    return small.crop((pad * S, pad * S, (pad + cov.width) * S, (pad + cov.height) * S))


def label(word, letters):
    """the word set as on the sheets (letters 2 game px apart, a 7 px space), narrowed sideways to the wheel's room if
    wider, then the light outline drawn once around it (2 game px) and the navy fill on top. RGBA at S x, with the
    baseline's row inside it"""
    parts, x = [], 0
    for c in word:
        if c == " ":
            x += 7
            continue
        kl, kr = KERN.get(c, (0, 0))
        x += kl
        cov, off = letters.coverage(c)
        parts.append((trace(cov), x, off))
        x += cov.width + kr
    width = x - 2
    top = min(o for _, _, o in parts)
    bot = max(o + p.height // S for p, _, o in parts)
    M = 6
    fill = Image.new("L", ((width + 2 * M) * S, (bot - top + 2 * M) * S), 0)
    for p, px_, off in parts:
        at = ((M + px_) * S, (M + off - top) * S)
        fill.paste(ImageChops.lighter(fill.crop((at[0], at[1], at[0] + p.width, at[1] + p.height)), p), at)
    if width > LABEL_MAXW:  # narrowed sideways, height kept (the game narrows its own long menu words the same way)
        sq = LABEL_MAXW / width
        fill = fill.resize((round((width * sq + 2 * M) * S), fill.height), Image.LANCZOS)
    # the outline: 2 game px around the fill, round (drawn at 2 x S, then area-averaged)
    big = fill.resize((fill.width * 2, fill.height * 2), Image.BILINEAR).point(lambda v: 255 if v >= 128 else 0)
    ring = big.filter(ImageFilter.GaussianBlur(1.5 * S * 2)).point(lambda v: 255 if v > 8 else 0)
    ring = ring.resize(fill.size, Image.BOX)
    out = Image.new("RGBA", fill.size, LINE + (0,))
    out.putalpha(ring)
    f = Image.new("RGBA", fill.size, FILL + (255,))
    f.putalpha(fill)
    out.alpha_composite(f)
    return out, (M - top) * S


def label_piece(word, letters):
    """the label in its place in the wheel piece's box (S x): centred, its baseline on the icons' label line"""
    lab, bl = label(word, letters)
    piece = Image.new("RGBA", (CELL[0] * S, CELL[1] * S), (0, 0, 0, 0))
    piece.alpha_composite(lab, (round(LABEL_CENTRE * S - lab.width / 2), LABEL_BASELINE * S - bl))
    return piece


# ---------------------------------------------------------------- the icon (code only, no disc data)
def icon():
    """the main menu in miniature, in the measured style of the wheel's icons: a 2 px light outline, an 8 px blue rim,
    the light fill, outer corner radius 15 and inner 6; inside, two outlined cards (43 x 52, 7 px strokes) and a row of
    a circle, a pill and a circle (20 tall, 6 px strokes): LOAD, OPTIONS, Quit, as on OrrePort's main menu. Drawn 4 x larger
    than the shipped picture and area-averaged down (the real icons' one-pixel soft steps)"""
    ss = S * 4
    im = Image.new("RGBA", (CELL[0] * ss, CELL[1] * ss), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)

    def rr(box, r, col):
        x0, y0, x1, y1 = box
        d.rounded_rectangle([x0 * ss, y0 * ss, x1 * ss - 1, y1 * ss - 1], radius=r * ss, fill=col)

    def outlined(box, r, stroke):
        x0, y0, x1, y1 = box
        rr(box, r, ICON_BLUE)
        rr((x0 + stroke, y0 + stroke, x1 - stroke, y1 - stroke), max(0, r - stroke), ICON_FILL)

    x0, y0, x1, y1 = FRAME
    rr(FRAME, 15, ICON_OUT)
    rr((x0 + 2, y0 + 2, x1 - 2, y1 - 2), 13, ICON_BLUE)
    rr((x0 + 10, y0 + 10, x1 - 10, y1 - 10), 6, ICON_FILL)
    outlined((x0 + 16, y0 + 18, x0 + 59, y0 + 70), 8, 7)
    outlined((x0 + 65, y0 + 18, x0 + 108, y0 + 70), 8, 7)
    # *Changed 2026-11-15 (snatcherLeo: "shouldn't the right circle (quit) be the exact same circle as the 'load' circle on the
    # left? Just adjust that"):* circle, pill, circle; the right circle the left one mirrored about the fill's centre
    # (62), and the pill widened to keep the same 4 px gap on both sides
    outlined((x0 + 16, y0 + 80, x0 + 36, y0 + 100), 10, 6)
    outlined((x0 + 40, y0 + 80, x0 + 84, y0 + 100), 10, 6)
    outlined((x0 + 88, y0 + 80, x0 + 108, y0 + 100), 10, 6)
    return im.resize((CELL[0] * S, CELL[1] * S), Image.BOX)


def save_png(img, path, marked):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    info = PngImagePlugin.PngInfo()
    if marked:  # traced from the discs: allowed in port/ui/traced/ only (the commit hook checks)
        info.add_text("Comment", MARKER + ": OrrePort's own traced outlines (tools/local/pc_menu_pictures.py)")
    img.save(path, pnginfo=info, optimize=True)
    print("wrote", path, img.size)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("us")
    ap.add_argument("eu")
    ap.add_argument("jp")
    ap.add_argument("--out", default=os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..")))
    ap.add_argument("--preview", help="a folder outside the repo for a preview sheet (the pieces on the wheel's blue)")
    a = ap.parse_args()
    sheets = read_sheets({"us": a.us, "eu": a.eu, "jp": a.jp})
    ic = icon()
    save_png(ic, os.path.join(a.out, "port", "ui", "game", "pc_menu", "main_menu_icon.png"), False)
    pieces = {}
    for lang, (word, src) in LABELS.items():
        pieces[lang] = label_piece(word, Letters(sheets, src))
        save_png(pieces[lang], os.path.join(a.out, "port", "ui", "traced", "pc_menu", f"main_menu_label_{lang}.png"), True)
    if a.preview:
        os.makedirs(a.preview, exist_ok=True)
        sheet = Image.new("RGBA", (CELL[0] * len(pieces), CELL[1] * 2), (40, 100, 190, 255))
        for i, (lang, p) in enumerate(pieces.items()):
            both = ic.copy()
            both.alpha_composite(p)
            sheet.alpha_composite(both.resize(CELL, Image.BOX), (i * CELL[0], 0))
        sheet = sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST)
        big = Image.new("RGBA", (ic.width * 2, ic.height), (40, 100, 190, 255))
        both = ic.copy()
        both.alpha_composite(pieces["en"])
        big.alpha_composite(both, (0, 0))
        big.alpha_composite(pieces["de"], (ic.width, 0))
        sheet.save(os.path.join(a.preview, "pc_menu_pictures_game_size_x2.png"))
        big.save(os.path.join(a.preview, "pc_menu_pictures_4x.png"))
        print("preview in", a.preview)


if __name__ == "__main__":
    main()
