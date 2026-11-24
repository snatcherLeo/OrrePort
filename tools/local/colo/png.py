"""Minimal PNG reader and image comparison (standard library only: no PIL / numpy on this machine).
Reads 8-bit greyscale / RGB / RGBA / grey+alpha, non-interlaced (what the PC build's frame dumps and extract.py write).
Used by the testing lane to compare frame dumps between runs."""
import struct
import zlib

CHANNELS = {0: 1, 2: 3, 4: 2, 6: 4}


def read_png(data):
    """(width, height, channels, rows): rows = list of bytes, one per scanline, `channels` bytes per pixel."""
    if data[:8] != b"\x89PNG\r\n\x1a\n": raise ValueError("not a PNG")
    pos, idat, ihdr = 8, [], None
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8]); body = data[pos + 8:pos + 8 + n]; pos += 12 + n
        if kind == b"IHDR": ihdr = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT": idat.append(body)
        elif kind == b"IEND": break
    w, h, depth, ctype, _, _, interlace = ihdr
    if depth != 8 or ctype not in CHANNELS or interlace: raise ValueError(f"unsupported PNG (depth {depth}, type {ctype}, interlace {interlace})")
    bpp = CHANNELS[ctype]; stride = w * bpp
    raw = zlib.decompress(b"".join(idat)); rows = []; prev = bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]; cur = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = cur[i - bpp] if i >= bpp else 0; b = prev[i]; c = prev[i - bpp] if i >= bpp else 0
            if f == 1: cur[i] = (cur[i] + a) & 0xFF
            elif f == 2: cur[i] = (cur[i] + b) & 0xFF
            elif f == 3: cur[i] = (cur[i] + ((a + b) >> 1)) & 0xFF
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                cur[i] = (cur[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        rows.append(bytes(cur)); prev = cur
    return w, h, bpp, rows


def compare(a, b, tolerance=0):
    """Compare two PNG byte strings. Returns dict(same_size, differing_pixels, total_pixels, max_delta, mean_delta):
    a pixel differs when any channel differs by more than `tolerance` (alpha ignored)."""
    wa, ha, ca, ra = read_png(a); wb, hb, cb, rb = read_png(b)
    if (wa, ha) != (wb, hb): return dict(same_size=False, differing_pixels=None, total_pixels=wa * ha, max_delta=None, mean_delta=None)
    n = min(ca, cb, 3); diff = 0; mx = 0; tot = 0
    for y in range(ha):
        xa, xb = ra[y], rb[y]
        if xa == xb: continue
        for x in range(wa):
            d = max(abs(xa[x * ca + k] - xb[x * cb + k]) for k in range(n))
            if d > tolerance: diff += 1
            mx = max(mx, d); tot += d
    return dict(same_size=True, differing_pixels=diff, total_pixels=wa * ha, max_delta=mx, mean_delta=tot / (wa * ha))
