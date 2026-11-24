"""GameCube GX texture decoding and the type-9 GStexture file (docs/engine/textures.md).
Read-only reader (no file output): a port candidate, see docs/tools.md "Readers"."""
import struct

def c565(v): return (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31, 255)


def c5a3(v):
    if v & 0x8000: return (((v >> 10) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, (v & 31) * 255 // 31, 255)
    return (((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17, ((v >> 12) & 7) * 255 // 7)


def cia8(v): return (v & 255, v & 255, v & 255, v >> 8)


# GS format code -> (GX name, block w, block h, bits per pixel)   (GStextureGetGXformat)
GXF = {0x00: ("C4", 8, 8, 4), 0x01: ("C8", 8, 4, 8), 0x30: ("C14X2", 4, 4, 16), 0x40: ("I4", 8, 8, 4),
       0x41: ("IA4", 8, 4, 8), 0x42: ("I8", 8, 4, 8), 0x43: ("IA8", 4, 4, 16), 0x44: ("RGB565", 4, 4, 16),
       0x45: ("RGBA8", 4, 4, 32), 0x90: ("RGB5A3", 4, 4, 16), 0xB0: ("CMPR", 8, 8, 4), 0xA0: ("I8", 8, 4, 8)}


def decode_gx(data, fmt, w, h, palette=None):
    """Decode GameCube tiled pixels to a flat RGBA bytearray (w*h*4)."""
    name, bw, bh, bpp = GXF[fmt]
    out = bytearray(w * h * 4)
    def put(x, y, c):
        if x < w and y < h: o = (y * w + x) * 4; out[o:o + 4] = bytes(c)
    bx_n = (w + bw - 1) // bw; by_n = (h + bh - 1) // bh
    p = 0
    for by in range(by_n):
        for bx in range(bx_n):
            x0, y0 = bx * bw, by * bh
            if name == "RGBA8":
                blk = data[p:p + 64]; p += 64
                for i in range(16):
                    a, r = blk[i * 2], blk[i * 2 + 1]; g, b = blk[32 + i * 2], blk[33 + i * 2]
                    put(x0 + i % 4, y0 + i // 4, (r, g, b, a))
            elif name in ("RGB5A3", "RGB565", "IA8", "C14X2"):
                for i in range(16):
                    v = struct.unpack(">H", data[p:p + 2])[0]; p += 2
                    c = c5a3(v) if name == "RGB5A3" else c565(v) if name == "RGB565" else cia8(v) if name == "IA8" else \
                        (palette[v & 0x3FFF] if palette and (v & 0x3FFF) < len(palette) else (255, 0, 255, 255))
                    put(x0 + i % 4, y0 + i // 4, c)
            elif name in ("I8", "IA4", "C8"):
                for i in range(32):
                    v = data[p]; p += 1
                    c = (v, v, v, 255) if name == "I8" else ((v & 15) * 17,) * 3 + ((v >> 4) * 17,) if name == "IA4" else \
                        (palette[v] if palette and v < len(palette) else (255, 0, 255, 255))
                    put(x0 + i % 8, y0 + i // 8, c)
            elif name in ("I4", "C4"):
                for i in range(32):
                    v = data[p]; p += 1
                    for half, nib in ((0, v >> 4), (1, v & 15)):
                        c = (nib * 17,) * 3 + (255,) if name == "I4" else \
                            (palette[nib] if palette and nib < len(palette) else (255, 0, 255, 255))
                        idx = i * 2 + half; put(x0 + idx % 8, y0 + idx // 8, c)
            elif name == "CMPR":
                for sub in range(4):
                    sx, sy = x0 + (sub % 2) * 4, y0 + (sub // 2) * 4
                    c0, c1, bits = struct.unpack(">HHI", data[p:p + 8]); p += 8
                    a, b = c565(c0), c565(c1)
                    if c0 > c1: pal = [a, b, tuple((2 * a[k] + b[k]) // 3 for k in range(3)) + (255,),
                                       tuple((a[k] + 2 * b[k]) // 3 for k in range(3)) + (255,)]
                    else: pal = [a, b, tuple((a[k] + b[k]) // 2 for k in range(3)) + (255,), (0, 0, 0, 0)]
                    for i in range(16):
                        put(sx + i % 4, sy + i // 4, pal[(bits >> (30 - 2 * i)) & 3])
    return out


def decode_texture_file(d):
    """Type-9 texture (GStextureHandle header + pixels) -> (w, h, rgba, gx name) or None."""
    w, h = struct.unpack(">HH", d[:4]); fmt = struct.unpack(">i", d[8:12])[0]
    if fmt not in GXF or not w or not h: return None
    tlut = struct.unpack(">I", d[0x0C:0x10])[0]
    off = struct.unpack(">I", d[0x28:0x2C])[0] or 0x80
    palette = None
    if GXF[fmt][0] in ("C4", "C8", "C14X2"):
        # palette offset. *Updated 2026-11-18:* likely right: it decoded XD's paletted (C8) mug-shot sheets correctly by
        # eye in Studio's read-only XD check (the USA Colosseum disc has no paletted type-9 files to test it on)
        po = struct.unpack(">I", d[0x48:0x4C])[0]
        conv = {0: cia8, 1: c565, 2: c5a3}.get(tlut, c5a3)
        palette = [conv(struct.unpack(">H", d[po + 2 * i:po + 2 * i + 2])[0]) for i in range(min(16384, (len(d) - po) // 2))] if po else None
    return w, h, decode_gx(d[off:], fmt, w, h, palette), GXF[fmt][0]


def crop(rgba, w, x, y, cw, ch):
    out = bytearray()
    for yy in range(y, y + ch): out += rgba[(yy * w + x) * 4:(yy * w + x + cw) * 4]
    return out

