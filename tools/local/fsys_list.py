"""List every .fsys archive on a GameCube disc image with its entries' names and types (read-only).
Usage: python -I fsys_list.py <image.iso> [--tsv]
  --tsv columns: archive, archive id, entries, name, type, entry id, lzss, size, stored size
Prints metadata only (names, type codes, sizes); never writes game data anywhere.

Archive header +0x08 = archive id (GSfsys.toc maps id -> name), +0x0C = entry count.
FSYS layout as read from the disc and src/game/fsys/fsys_file.c (docs/engine/disc-files.md):
  header +0x0C u32 entry count, +0x18 u32 offset of the offset block;
  offset block +0x00 entry-offset table, +0x04 name strings, +0x08 data start;
  entry (0x50 bytes) +0x00 u32 id (u16 id, u8 type*2, u8 0), +0x04 data offset, +0x08 size,
  +0x0C flags (0x80000000 = LZSS), +0x14 stored size, +0x20 type code, +0x24 name offset."""
import struct, sys

iso = open(sys.argv[1], "rb")
tsv = "--tsv" in sys.argv
def rd(off, n): iso.seek(off); return iso.read(n)
fst_off, fst_size = struct.unpack(">II", rd(0x424, 8))
fst = rd(fst_off, fst_size)
n = struct.unpack(">I", fst[8:12])[0]
strtab = fst[n * 12:]
def cstr(buf, o): return buf[o:buf.index(b"\0", o)].decode("ascii", "replace")
ends = []
for i in range(1, n):
    while ends and i >= ends[-1][0]: ends.pop()
    w0, a, b = struct.unpack(">III", fst[i * 12:i * 12 + 12])
    nm = cstr(strtab, w0 & 0xFFFFFF)
    base = ends[-1][1] if ends else ""
    full = f"{base}/{nm}" if base else nm
    if w0 >> 24:
        ends.append((b, full)); continue
    if not nm.lower().endswith(".fsys"): continue
    h = rd(a, 0x60)
    if h[:4] != b"FSYS":
        print(f"{full}\tNOT-FSYS"); continue
    aid, count = struct.unpack(">II", h[0x08:0x10])
    blk = struct.unpack(">I", h[0x18:0x1C])[0]
    data_start = struct.unpack(">I", rd(a + blk, 12)[8:12])[0]
    hdr = rd(a, max(data_start, 0x60))
    tab, names = struct.unpack(">II", hdr[blk:blk + 8])
    rows = []
    for k in range(count):
        eo = struct.unpack(">I", hdr[tab + 4 * k:tab + 4 * k + 4])[0]
        e = hdr[eo:eo + 0x50]
        ident, doff, size, flags = struct.unpack(">IIII", e[0:16])
        stored = struct.unpack(">I", e[0x14:0x18])[0]
        ftype, noff = struct.unpack(">II", e[0x20:0x28])
        rows.append((cstr(hdr, noff), ftype, ident >> 16, bool(flags & 0x80000000), size, stored))
    if tsv:
        if not rows:  # empty archive: one placeholder row (type -1)
            print(f"{full}\t{aid:#06x}\t0\t\t-1\t\t0\t0\t0")
        for r in rows:
            print(f"{full}\t{aid:#06x}\t{count}\t{r[0]}\t{r[1]}\t{r[2]:#06x}\t{int(r[3])}\t{r[4]}\t{r[5]}")
    else:
        print(f"{full}  (archive id {aid:#06x}, {b} bytes, {count} entries)")
        for r in rows:
            print(f"    {r[0]:40} type {r[1]:2d}  id {r[2]:#06x}  {'lzss' if r[3] else 'raw '}  {r[4]}")
