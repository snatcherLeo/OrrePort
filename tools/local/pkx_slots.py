"""List every PKX battle model's animation-slot table as markdown rows (read-only; no game data out).
Usage: python -I pkx_slots.py <image.iso>
Each row: model name, slot count, and for slots 0-16 the HSD animation numbers the slot plays
('-' = empty slot, 'x' = slot with 'no animation'). Layout: docs/engine/models-animation.md."""
import os, struct, sys
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fsys_extract.py"), encoding="utf-8").read()
exec(src[src.index("def lzss"):src.index("iso = open")])
iso = open(sys.argv[1], "rb")
def rd(o, n): iso.seek(o); return iso.read(n)
fo, fs = struct.unpack(">II", rd(0x424, 8)); fst = rd(fo, fs)
n = struct.unpack(">I", fst[8:12])[0]; st = fst[n * 12:]
rows = []
for i in range(1, n):
    w0, a, b = struct.unpack(">III", fst[i * 12:i * 12 + 12])
    nm = st[w0 & 0xFFFFFF:st.index(b"\0", w0 & 0xFFFFFF)].decode()
    if w0 >> 24 or not nm.startswith("pkx_"): continue
    arc = rd(a, b); blk = struct.unpack(">I", arc[0x18:0x1C])[0]; tab = struct.unpack(">I", arc[blk:blk + 4])[0]
    eo = struct.unpack(">I", arc[tab:tab + 4])[0]; e = arc[eo:eo + 0x50]
    doff, usize, flags = struct.unpack(">III", e[4:16]); stored = struct.unpack(">I", e[0x14:0x18])[0]
    raw = arc[doff:doff + stored]; d = lzss(raw) if flags & 0x80000000 else raw
    h = struct.unpack(">3I", d[:12])
    base = 0x40 + ((h[0] + 0x1F) & ~0x1F) + ((h[1] + 0x1F) & ~0x1F)
    cells = []
    for k in range(17):
        if k >= h[2]: cells.append(""); continue
        r = d[base + k * 0xD0: base + (k + 1) * 0xD0]
        ca, cb = struct.unpack(">II", r[:8])
        pairs = [struct.unpack(">II", r[0x8C + 8 * j:0x94 + 8 * j]) for j in range(min(cb, 8))]
        if ca == 0 and struct.unpack(">i", r[0x4C:0x50])[0] == -1: cells.append("-")
        elif pairs and all(t == 1 for t, v in pairs): cells.append("x")
        else: cells.append("+".join(str(v) for t, v in pairs if t == 0))
    rows.append(f"| {nm[4:-5]} | {h[2]} | " + " | ".join(cells) + " |")
print("| Model | Slots | " + " | ".join(str(k) for k in range(17)) + " |")
print("|---|---|" + "---|" * 17)
print("\n".join(sorted(rows)))
