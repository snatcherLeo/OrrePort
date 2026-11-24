"""GFL lens-flare resource (resource type 17; the one on the USA disc is in T1_ancient_colo) (docs/engine/lens-flare.md).
Read-only reader (no file output): a port candidate. Layout from flareInit (src/game/battle/battle_range_exact_801ED640.c)
and flareDraw (battle_range_candidate_801ED780.c)."""
import struct

from .gx import decode_texture_file

MAGIC = 0x7B1EE3F0


def parse_flare(b):
    """Returns {far, mid, near, glare: (f32, f32), elements: [{type, texture, position, scale}], textures: [{id,
    offset, image: (w, h, rgba, format) or None}]} or None when the magic does not match."""
    magic, ntex, nelem, far, mid, near, g0, g1 = struct.unpack_from(">IHHIIIff", b, 0)
    if magic != MAGIC: return None
    els = []
    for i in range(nelem):
        t, _, tex, pos, sc = struct.unpack_from(">BBHff", b, 0x1C + 12 * i)
        els.append({"type": t, "texture": tex, "position": pos, "scale": sc})
    texs = []
    for i in range(ntex):
        tid, _, off = struct.unpack_from(">HHI", b, 0x1C + 12 * nelem + 8 * i)
        texs.append({"id": tid, "offset": off, "image": decode_texture_file(b[off:])})
    return {"far": far, "mid": mid, "near": near, "glare": (g0, g1), "elements": els, "textures": texs}


def listing(f, name=""):
    L = [f"# {name}: lens flare", "",
         f"Sun distance from the screen centre (likely screen pixels): fades in from {f['far']} to {f['mid']}; the full-screen glare grows "
         f"from {f['mid']} to {f['near']}; glare strength {f['glare'][0]:g} or {f['glare'][1]:g} (the second is used unless "
         "fn_800D2F34 reports 2).", "", "| Element | Type | Texture | Position along the sun-centre line | Scale |", "|---|---|---|---|---|"]
    for i, e in enumerate(f["elements"]):
        kind = "on the sun" if e["type"] == 1 else "along the line"
        L.append(f"| {i} | {e['type']} ({kind}) | {e['texture']} | {e['position']:g} | {e['scale']:g} |")
    L += ["", "Textures: " + ", ".join(f"{t['id']} ({t['image'][0]}x{t['image'][1]} {t['image'][3]})" if t["image"] else f"{t['id']} (?)" for t in f["textures"])]
    return "\n".join(L)
