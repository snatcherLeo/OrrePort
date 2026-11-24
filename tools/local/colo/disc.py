"""GameCube disc image: file table, files, main.dol (docs/engine/disc-files.md).
Read-only reader (no file output): a port candidate, see docs/tools.md "Readers"."""
import struct

class Disc:
    def __init__(self, path):
        self.f = open(path, "rb")
        fo, fs = struct.unpack(">II", self.read(0x424, 8))
        fst = self.read(fo, fs)
        n = struct.unpack(">I", fst[8:12])[0]
        st = fst[n * 12:]
        self.files = {}  # path -> (offset, size)
        ends = []
        for i in range(1, n):
            while ends and i >= ends[-1][0]: ends.pop()
            w0, a, b = struct.unpack(">III", fst[i * 12:i * 12 + 12])
            nm = st[w0 & 0xFFFFFF:st.index(b"\0", w0 & 0xFFFFFF)].decode("ascii", "replace")
            full = f"{ends[-1][1]}/{nm}" if ends else nm
            if w0 >> 24: ends.append((b, full))
            else: self.files[full] = (a, b)
    def read(self, off, n):
        self.f.seek(off); return self.f.read(n)
    def dol(self):
        """main.dol: its disc offset is at 0x420 of the disc header; size from its section table."""
        off = struct.unpack(">I", self.read(0x420, 4))[0]; h = self.read(off, 0x100)
        offs = struct.unpack(">18I", h[0:72]); sizes = struct.unpack(">18I", h[144:216])
        return self.read(off, max(o + z for o, z in zip(offs, sizes)))
    def file(self, path):
        a, b = self.files[path]; return self.read(a, b)



def parse_banner(b):
    """The disc banner (opening.bnr, 'BNR1': one language; standard GameCube layout, docs/engine/disc-files.md):
    0x20 header, 96x32 RGB5A3 image (0x1800 bytes), then short name (0x20), short maker (0x20), long name (0x40),
    long maker (0x40), description (0x80); USA text is Latin-1. Returns {image: (w, h, rgba), texts} or None."""
    from .gx import decode_gx
    if b[:4] not in (b"BNR1", b"BNR2"): return None
    def s(o, n): return b[o:o + n].split(b"\0", 1)[0].decode("latin-1")
    return {"image": (96, 32, decode_gx(b[0x20:0x1820], 0x90, 96, 32)),
            "texts": {"short name": s(0x1820, 0x20), "short maker": s(0x1840, 0x20), "long name": s(0x1860, 0x40),
                      "long maker": s(0x18A0, 0x40), "description": s(0x18E0, 0x80)}}


def gba_header(b):
    """A GBA program's cartridge / multiboot header (standard GBA layout, the one pkjb_uploader's PKJB_Prepare patches):
    0xA0 title (12 bytes), 0xAC game code (4), 0xB0 maker (2), 0xB2 fixed 0x96, 0xBD header checksum
    (0xE7 minus every byte of 0xA0..0xBC, as PKJB_Prepare recomputes it after writing its key into 0xAC..0xAF).
    Returns a dict or None when the fixed byte is missing (not a GBA header, e.g. the bg*thumbcode.bin blocks: the e-Reader program, sent behind a
    0x34-byte header the game builds at run time, fn_8007B350; docs/tools.md GBA row)."""
    if len(b) < 0xC0 or b[0xB2] != 0x96: return None
    s = (0xE7 - sum(b[0xA0:0xBD])) & 0xFF
    t = lambda o, n: b[o:o + n].split(b"\0", 1)[0].decode("ascii", "replace")
    return {"title": t(0xA0, 12), "game_code": t(0xAC, 4), "maker": t(0xB0, 2), "checksum": b[0xBD],
            "checksum_ok": s == b[0xBD], "entry_branch": int.from_bytes(b[0:4], "little")}
