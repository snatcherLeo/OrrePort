"""HSD (.dat) models: textures inside a model, found by scanning image descriptors (docs/engine/extractor.md).
Read-only reader (no file output): a port candidate, see docs/tools.md "Readers"."""
import struct
from .gx import GXF, decode_gx, c565, c5a3, cia8
from .tables import GX_TO_GS

def hsd_textures(dat):
    """Find the textures inside an HSD .dat (TObj image descriptors, Melee layout). Yields (w, h, gx, rgba).
    Heuristic: every relocated pointer is tried as an ImageDesc {u32 image, u16 w, u16 h, u32 gx format,
    u32 mipmap, f32 minLOD, f32 maxLOD}; TObjs (image desc at +0x4C, palette at +0x50) supply palettes."""
    if len(dat) < 0x20: return
    fsize, dsize, nrel = struct.unpack(">III", dat[:12])
    data = dat[0x20:0x20 + dsize]
    rel = [struct.unpack(">I", dat[0x20 + dsize + 4 * i:0x24 + dsize + 4 * i])[0] for i in range(nrel)]
    relset = set(rel)
    def u32(o): return struct.unpack(">I", data[o:o + 4])[0] if 0 <= o <= len(data) - 4 else 0
    tlut_of = {}
    for o in rel:  # TObj candidates: pointer at +0x4C to an image desc, optional palette desc at +0x50
        t = o - 0x4C
        if t >= 0 and (t + 0x50) in relset: tlut_of[u32(o)] = u32(t + 0x50)
    seen = set()
    for o in rel:
        if o + 0x18 > len(data): continue
        img = u32(o); w, h = struct.unpack(">HH", data[o + 4:o + 8]); gx = u32(o + 8)
        if gx not in GX_TO_GS or not (1 <= w <= 1024 and 1 <= h <= 1024) or img in seen or img >= len(data): continue
        bw, bh, bpp = GXF[GX_TO_GS[gx]][1:]
        need = ((w + bw - 1) // bw) * ((h + bh - 1) // bh) * bw * bh * bpp // 8
        if img + need > len(data) or u32(o + 12) > 1: continue
        palette = None
        if GX_TO_GS[gx] in (0x00, 0x01, 0x30):
            td = tlut_of.get(o)
            if not td: continue
            pdata, pfmt = u32(td), u32(td + 4); n = struct.unpack(">H", data[td + 12:td + 14])[0]
            conv = {0: cia8, 1: c565, 2: c5a3}.get(pfmt, c5a3)
            palette = [conv(struct.unpack(">H", data[pdata + 2 * i:pdata + 2 * i + 2])[0]) for i in range(n)]
        seen.add(img)
        try: yield w, h, GXF[GX_TO_GS[gx]][0], decode_gx(data[img:], GX_TO_GS[gx], w, h, palette)
        except Exception: continue



def hsd_animations(dat):
    """The animation list of every model set in an HSD .dat (not the PKX slot table, which points into it) (docs/engine/hsd.md, "Model sets and their animations").
    Returns [[(slot, end_frame), ...] per model set], or [] if the archive has no scene_data root.
    Layout (verified from data, A 2026-10-15): scene_data +0 is a null-terminated list of model sets; a model set holds
    the root joint at +0 and a null-terminated list of joint-animation trees (HSD_AnimJoint) at +4. An animation's
    length is the largest end frame of the AObjDesc (+8 of each AnimJoint: child +0, next +4) in its tree."""
    if len(dat) < 0x20:
        return []
    fsize, dsize, nrel, nroot, nref = struct.unpack(">5I", dat[:20])
    data = dat[0x20:0x20 + dsize]
    rt = 0x20 + dsize + 4 * nrel
    st = rt + 8 * (nroot + nref)
    root = None
    for i in range(nroot):
        off, so = struct.unpack(">II", dat[rt + 8 * i:rt + 8 * i + 8])
        end = dat.index(b"\0", st + so)
        if dat[st + so:end] == b"scene_data":
            root = off
    if root is None:
        return []

    def u32(o):
        return struct.unpack(">I", data[o:o + 4])[0] if 0 <= o <= len(data) - 4 else 0

    def f32(o):
        return struct.unpack(">f", data[o:o + 4])[0] if 0 <= o <= len(data) - 4 else 0.0

    def tree_end(aj, depth=0):
        best, seen = 0.0, 0
        while aj and seen < 4096 and depth < 256:
            ad = u32(aj + 8)
            if ad:
                best = max(best, f32(ad + 4))
            child = u32(aj)
            if child:
                best = max(best, tree_end(child, depth + 1))
            aj = u32(aj + 4)
            seen += 1
        return best

    out = []
    sets = u32(root)
    k = 0
    while sets and k < 64 and u32(sets + 4 * k):
        ms = u32(sets + 4 * k)
        anims = u32(ms + 4)
        slots = []
        j = 0
        while anims and j < 256 and u32(anims + 4 * j):
            slots.append((j, tree_end(u32(anims + 4 * j))))
            j += 1
        out.append(slots)
        k += 1
    return out
