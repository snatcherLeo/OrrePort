"""List every .thp file on a GameCube disc image with its THP header info (read-only).
Usage: python -I disc_thp.py <image.iso>
GameCube disc: FST offset/size at 0x424/0x428. THP header: magic "THP\0", version, ..., f32 frame rate
at 0x10, u32 frame count at 0x14. Output: path, size, frames, fps, duration."""
import struct, sys
iso = open(sys.argv[1], "rb")
def rd(off, n): iso.seek(off); return iso.read(n)
fst_off, fst_size = struct.unpack(">II", rd(0x424, 8))
fst = rd(fst_off, fst_size)
n = struct.unpack(">I", fst[8:12])[0]
strtab = fst[n * 12:]
def name(o): return strtab[o:strtab.index(b"\0", o)].decode("ascii", "replace")
paths = [""] * n; ends = []  # directory stack of (end index, path)
rows = []
for i in range(1, n):
    while ends and i >= ends[-1][0]: ends.pop()
    w0, a, b = struct.unpack(">III", fst[i * 12:i * 12 + 12])
    isdir, nm = w0 >> 24, name(w0 & 0xFFFFFF)
    base = ends[-1][1] if ends else ""
    full = f"{base}/{nm}" if base else nm
    if isdir:
        ends.append((b, full))
    elif nm.lower().endswith(".thp"):
        h = rd(a, 0x30)
        if h[:4] == b"THP\0":
            fps = struct.unpack(">f", h[0x10:0x14])[0]; frames = struct.unpack(">I", h[0x14:0x18])[0]
        else:
            fps, frames = 0.0, 0
        rows.append((full, b, frames, fps))
for full, size, frames, fps in rows:
    dur = frames / fps if fps else 0
    print(f"{full:40} {size/1048576:8.1f} MB  {frames:6d} frames  {fps:5.2f} fps  {int(dur // 60)}:{dur % 60:05.2f}")
print(f"{len(rows)} .thp files")
