"""FSYS archives and LZSS (docs/engine/disc-files.md).
Read-only reader (no file output): a port candidate, see docs/tools.md "Readers"."""
import struct

def lzss(src):
    out = bytearray(); win = bytearray(0x1000); w = 0xFEE; i = 0x10; flags = 0
    n = len(src)
    while i < n:
        flags >>= 1
        if not flags & 0x100:
            flags = src[i] | 0xFF00; i += 1
            if i >= n: break
        if flags & 1:
            b = src[i]; i += 1; out.append(b); win[w] = b; w = (w + 1) & 0xFFF
        else:
            if i + 1 >= n: break
            a, b = src[i], src[i + 1]; i += 2
            off = a | ((b << 4) & 0xF00)
            for k in range((b & 0xF) + 3):
                c = win[(off + k) & 0xFFF]; out.append(c); win[w] = c; w = (w + 1) & 0xFFF
    return bytes(out)


def fsys_entries(arc):
    """Yield (index, name, type, entry_id_u32, unpacked bytes loader) for one FSYS archive."""
    if arc[:4] != b"FSYS": return
    count = struct.unpack(">I", arc[0x0C:0x10])[0]
    blk = struct.unpack(">I", arc[0x18:0x1C])[0]
    tab = struct.unpack(">I", arc[blk:blk + 4])[0]
    for k in range(count):
        eo = struct.unpack(">I", arc[tab + 4 * k:tab + 4 * k + 4])[0]
        e = arc[eo:eo + 0x50]
        ident, doff, usize, flags = struct.unpack(">IIII", e[:16])
        stored = struct.unpack(">I", e[0x14:0x18])[0]
        ftype, noff = struct.unpack(">II", e[0x20:0x28])
        name = arc[noff:arc.index(b"\0", noff)].decode("ascii", "replace")
        def load(doff=doff, stored=stored, flags=flags):
            raw = arc[doff:doff + stored]
            return lzss(raw) if flags & 0x80000000 else raw
        yield k, name, ftype, ident, load


def archives(disc):
    for path in sorted(disc.files):
        if path.endswith(".fsys"): yield path[:-5], disc.file(path)

