"""Fonts (resource type 6 files, and the main fonts inside common_rel): glyph tables and 4-bit bitmaps
(docs/engine/messages.md, "Fonts"). Layout from the game's glyph code: fn_800FD348 (lookup and
call) and fn_800FD69C (copy into the 512x512 I4 glyph texture) in src/game/gs_msg.c.
Read-only reader (no file output): a port candidate, see docs/tools.md "Readers"."""
import struct

# common_rel resource table (tableResBiosGetResPtr) entries that hold fonts: section-4 offsets.
REL_FONTS = [("common_rel_font0", 0x2d2c), ("common_rel_font2", 0x45a94), ("common_rel_font3", 0x494e4)]


def parse_fonts(data):
    """Parse a font file: a list of fonts, ended by font id 0xFFFF (or the end of the data).

    Font header (0x18 bytes): u16 font id, u8 cell width, u8 cell height, u32 font size, u16 glyph
    count, u16 0, u32 data offset (counted from header + 8), then the glyph records.
    Glyph record (8 bytes, sorted by character): u16 character (Unicode), u8 width (= the advance),
    u8 height, s8 vertical offset (added to the baseline when drawing), u24 bitmap offset.
    Bitmap: 4-bit intensity, rows of ceil(width / 2) bytes, high nibble first.
    Returns [{id, cell_w, cell_h, glyphs: [{char, width, height, y_offset, pixels}]}]."""
    fonts = []; o = 0
    while o + 0x18 <= len(data):
        fid, cw, ch, size, count = struct.unpack(">HBBIH", data[o:o + 10])
        if fid == 0xFFFF or not count or not cw or not ch: break  # size 0 = a single font running to the end of the data (jis_1)
        doff = struct.unpack(">I", data[o + 0x0C:o + 0x10])[0]
        if o + 0x18 + 8 * count > len(data) or o + 8 + doff > len(data): break  # padding / not a font header
        glyphs = []
        for g in range(count):
            r = data[o + 0x18 + 8 * g:o + 0x20 + 8 * g]
            char, w, h = struct.unpack(">HBB", r[:4])
            yoff = struct.unpack(">b", r[4:5])[0]; off = int.from_bytes(r[5:8], "big")
            stride = (w + 1) // 2; base = o + 8 + doff + off; rows = []
            for y in range(h):
                row = data[base + y * stride:base + (y + 1) * stride]; px = []
                for b in row: px += [b >> 4, b & 15]
                rows.append(px[:w])
            glyphs.append({"char": char, "width": w, "height": h, "y_offset": yoff, "pixels": rows})
        fonts.append({"id": fid, "cell_w": cw, "cell_h": ch, "glyphs": glyphs})
        if not size: break
        # the next font starts exactly at this header + size, as the game steps (GSmsgFontOpen: file + nextOffset).
        # *Corrected 2026-11-13:* this rounded up to 32 bytes, which lost the main font file's chain after its
        # first font (the second starts at 0x14ED0), so fonts 1, 3, 2 and 4 of that file were never extracted.
        o += size
    return fonts


def line_gap(font_id, cell_h):
    """Extra pixels between lines (GSmsgSetFontInfo, gs_msg.c, verified): 11 for font 0, 6 for font 1, otherwise
    int(cell height / 2 + 1)."""
    return 11 if font_id == 0 else 6 if font_id == 1 else int(0.5 * cell_h + 1.0)


def metrics(font):
    """Layout metrics the game uses for this font (all verified from gs_msg.c / msgctrl.c):
    - a glyph advances the pen by its own width (no extra letter spacing; msgSetChar);
    - a space (U+0020) is never looked up: it advances int(cell width / 2) (msgSetChar);
    - a character no registered font has draws a white box of the cell size and advances cell width + 2;
    - a new line (control code CR) moves down by cell height + line_gap (msgctrlCR);
    all of these times the text scale (1.0 unless a caller changes it)."""
    gap = line_gap(font["id"], font["cell_h"])
    return {"font_id": font["id"], "cell_width": font["cell_w"], "cell_height": font["cell_h"],
            "line_gap": gap, "line_height": font["cell_h"] + gap, "letter_spacing": 0,
            "space_advance": font["cell_w"] // 2, "missing_glyph_advance": font["cell_w"] + 2,
            "glyph_count": len(font["glyphs"])}


def atlas(font, columns=32, pad=2, grid=True):
    """Lay a font's glyphs out on a grid (glyph order): returns (w, h, rgba), light glyphs on a dark
    background, each glyph drawn at its vertical offset inside its cell; grid=True outlines each cell and marks
    each glyph's advance width with a short tick at the cell bottom."""
    cw, ch = font["cell_w"] + pad, font["cell_h"] + pad + 4
    n = len(font["glyphs"]); rows = (n + columns - 1) // columns
    W, H = columns * cw, max(rows, 1) * ch
    out = bytearray([40, 40, 48, 255] * (W * H))
    if grid:
        for i in range(max(rows, 1) * columns):
            x0, y0 = (i % columns) * cw, (i // columns) * ch
            for x in range(x0, min(x0 + cw, W)):
                o = (y0 * W + x) * 4; out[o:o + 4] = bytes((70, 70, 92, 255))
            for y in range(y0, min(y0 + ch, H)):
                o = (y * W + x0) * 4; out[o:o + 4] = bytes((70, 70, 92, 255))
            if i < n:  # advance tick: the glyph's width along the cell bottom
                yy = min(y0 + ch - 2, H - 1)
                for x in range(x0 + pad // 2, min(x0 + pad // 2 + font["glyphs"][i]["width"], W)):
                    o = (yy * W + x) * 4; out[o:o + 4] = bytes((200, 120, 60, 255))
    for i, g in enumerate(font["glyphs"]):
        x0, y0 = (i % columns) * cw + pad // 2, (i // columns) * ch + pad // 2 + 2 + g["y_offset"]
        for y, row in enumerate(g["pixels"]):
            for x, v in enumerate(row):
                yy, xx = y0 + y, x0 + x
                if v and 0 <= yy < H and 0 <= xx < W:
                    o = (yy * W + xx) * 4; a = v * 17
                    out[o:o + 4] = bytes((40 + (215 * a) // 255, 40 + (215 * a) // 255, 48 + (207 * a) // 255, 255))
    return W, H, out


# Code points the game uses for its own symbols instead of the standard character (docs/engine/fonts.md, "Special
# symbols"; found by a scan of all 221 fonts on the USA disc, 2026-10-16). (code point, file-name tag, what it shows)
SPECIAL_SYMBOLS = [
    (0x0024, "pokedollar", "the Pokemon dollar sign: a P with two horizontal bars"),
    (0xFF04, "pokedollar_fullwidth", "the Pokemon dollar sign, full-width"),
    (0x2031, "speech_bubble", "a solid speech bubble with a tail at the lower right"),
    (0x03B2, "ereader_e", "a bold lower-case e in the e-Reader logo style"),
    (0x03B1, "ereader_e_plus", "the same e with a small plus sign at its upper right (e+)"),
    (0x337C, "ereader_e_plus_2", "the same e+ mark (identical to U+03B1)"),
    (0x337D, "ereader_e_2", "the same e mark (identical to U+03B2)"),
    (0x337E, "e_acute", "a lower-case e with an acute accent"),
    (0x2030, "two_low_dots", "two small dots on the baseline"),
    (0x25A1, "boxed_s", "a capital S inside a square frame"),
    (0xFF3E, "small_ring", "a small ring in the upper part of the cell (handakuten-like)"),
    (0x005C, "yen", "a yen sign"),
]


def glyph_image(glyph, cell_h, scale=8, pad=2):
    """One glyph on a white cell (cell_h + pad on each side), dark ink, enlarged `scale` times with hard pixel edges.
    The glyph's top sits at its vertical offset. Returns (w, h, rgba)."""
    w = max(glyph["width"], 1) + 2 * pad; h = cell_h + 2 * pad
    px = [[255] * w for _ in range(h)]
    for y, row in enumerate(glyph["pixels"]):
        for x, v in enumerate(row):
            yy = pad + glyph["y_offset"] + y
            if 0 <= yy < h and v: px[yy][pad + x] = 255 - v * 17
    out = bytearray()
    for row in px:
        line = b"".join(bytes((v, v, v, 255)) * scale for v in row)
        out += line * scale
    return w * scale, h * scale, out


def label_sheet(entries, label_font, scale=4, columns=4, pad=8):
    """Overview sheet: each entry (glyph, cell_h, label text) as an enlarged glyph with its label underneath, drawn with
    the game's own main font (label_font: a parsed font whose glyphs include ASCII). Returns (w, h, rgba)."""
    lab = {g["char"]: g for g in label_font["glyphs"]}; lh = label_font["cell_h"]
    def text_width(t): return sum(lab[ord(c)]["width"] if ord(c) in lab and c != " " else label_font["cell_w"] // 2 for c in t)
    cell_w = max(max(text_width(t) for _, _, t in entries), max(28 * scale, 1)) + 2 * pad
    cell_h = 30 * scale + lh + 3 * pad
    rows = (len(entries) + columns - 1) // columns; W, H = columns * cell_w, rows * cell_h
    img = [[255] * W for _ in range(H)]
    for i, (g, ch, text) in enumerate(entries):
        x0, y0 = (i % columns) * cell_w + pad, (i // columns) * cell_h + pad
        for y in range(y0 - pad // 2, min(y0 - pad // 2 + cell_h - 1, H)):  # frame
            for x in (x0 - pad // 2, min(x0 - pad // 2 + cell_w - 2, W - 1)): img[y][x] = 200
        for y, row in enumerate(g["pixels"]):
            for x, v in enumerate(row):
                for dy in range(scale):
                    for dx in range(scale):
                        yy, xx = y0 + (2 + g["y_offset"] + y) * scale + dy, x0 + x * scale + dx
                        if v and 0 <= yy < H and 0 <= xx < W: img[yy][xx] = 255 - v * 17
        tx, ty = x0, y0 + 28 * scale + pad
        for c in text:
            if c == " " or ord(c) not in lab: tx += label_font["cell_w"] // 2; continue
            lg = lab[ord(c)]
            for y, row in enumerate(lg["pixels"]):
                for x, v in enumerate(row):
                    yy, xx = ty + lg["y_offset"] + y, tx + x
                    if v and 0 <= yy < H and 0 <= xx < W: img[yy][xx] = min(img[yy][xx], 255 - v * 17)
            tx += lg["width"]
    out = bytearray()
    for row in img:
        for v in row: out += bytes((v, v, v, 255))
    return W, H, out
