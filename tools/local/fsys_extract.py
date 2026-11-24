"""Unpack one entry of one .fsys archive from the player's own disc image into extracted/ (git-ignored).
Usage: python -I fsys_extract.py <image.iso> <archive.fsys> <entry name or index> [out_dir, default extracted/raw_entries]
LZSS as in FSYSDecompressLZSS (gs_range_8017F2C4.c): 16-byte "LZSS" header, 4 KB window starting
at 0xFEE, flag bit 1 = literal, 0 = (offset 12 bits, length 4 bits + 3) back-reference.
Never commit the output (the project rules)."""
import os, struct, sys

def lzss(src):
    out = bytearray(); win = bytearray(0x1000); w = 0xFEE; i = 0x10; flags = 0
    while i < len(src):
        flags >>= 1
        if not flags & 0x100:
            flags = src[i] | 0xFF00; i += 1
            if i >= len(src): break
        if flags & 1:
            b = src[i]; i += 1; out.append(b); win[w] = b; w = (w + 1) & 0xFFF
        else:
            if i + 1 >= len(src): break
            a, b = src[i], src[i + 1]; i += 2
            off = a | ((b << 4) & 0xF00)
            for k in range((b & 0xF) + 3):
                c = win[(off + k) & 0xFFF]; out.append(c); win[w] = c; w = (w + 1) & 0xFFF
    return bytes(out)

iso = open(sys.argv[1], "rb")
def rd(o, n): iso.seek(o); return iso.read(n)
fo, fs = struct.unpack(">II", rd(0x424, 8)); fst = rd(fo, fs)
n = struct.unpack(">I", fst[8:12])[0]; st = fst[n * 12:]
for i in range(1, n):
    w0, a, b = struct.unpack(">III", fst[i * 12:i * 12 + 12])
    if not w0 >> 24 and st[w0 & 0xFFFFFF:].startswith(sys.argv[2].encode() + b"\0"): base, size = a, b
arc = rd(base, size)
count = struct.unpack(">I", arc[0x0C:0x10])[0]; blk = struct.unpack(">I", arc[0x18:0x1C])[0]
tab = struct.unpack(">I", arc[blk:blk + 4])[0]
for k in range(count):
    eo = struct.unpack(">I", arc[tab + 4 * k:tab + 4 * k + 4])[0]
    e = arc[eo:eo + 0x50]
    doff, usize, flags = struct.unpack(">III", e[4:16]); stored = struct.unpack(">I", e[0x14:0x18])[0]
    ftype, noff = struct.unpack(">II", e[0x20:0x28])
    name = arc[noff:arc.index(b"\0", noff)].decode()
    if sys.argv[3] in (name, str(k)):
        raw = arc[doff:doff + stored]
        data = lzss(raw) if flags & 0x80000000 else raw
        out_dir = sys.argv[4] if len(sys.argv) > 4 else os.path.join("extracted", "raw_entries")
        os.makedirs(out_dir, exist_ok=True)
        path = os.path.join(out_dir, f"{sys.argv[2][:-5]}__{k:02d}_{name}.t{ftype}")
        open(path, "wb").write(data)
        print(path, len(data), "bytes (expected", usize, ")")
        break
