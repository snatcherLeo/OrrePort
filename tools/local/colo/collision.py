"""Room collision (CCD files, resource type 3, `.col`): docs/engine/collision.md.
Layout from the loader's pointer fix-up `_offsetCCD__FP12CCD_FILEHEAD` and the debug drawer `GScolsys2Draw` /
`fn_8010D20C` (src/game/gs_range_8010CBD0.c).
Read-only reader (no file output): a port candidate, see docs/tools.md "Readers"."""
import struct

# record fields holding a group, with the triangle stride the debug drawer uses and how it draws them
GROUPS = [  # (record offset, name, stride, used by)
    (0x24, "floor", 0x34, "walkable floor: height and layer queries (GScolsys2WalkGetHeight, GScolsys2WalkGetLayer)"),
    (0x28, "walls_player", 0x34, "walls for the player and partner (colsysWallAll(.., 0) via colsysWallSweep; hero_move.c)"),
    (0x2C, "pass_events", 0x34, "walk-through event triggers, event id at +0x32 (colsysPassGetEventID, GScolsys2ThruGetEventList)"),
    (0x30, "check_events", 0x34, "examine (A button) event planes, event id = u16 at +0x30 (GScolsys2Check -> fn_80111864)"),
    (0x34, "walls_npc", 0x34, "walls for NPCs (colsysWallAll(.., 1) via fn_8010F188; people.c)"),
    (0x38, "group_38", 0x30, "unknown: only the debug drawer reads it so far"),
]


def u32(d, o): return struct.unpack(">I", d[o:o + 4])[0]


def parse(d):
    """-> list of objects: {index, translate, rotate, scale, flags, groups: {name: {count, triangles, grid}}}.
    flags bit 0 = moving object: queries transform by its own matrix (fn_8010CA30) instead of using the grid.
    A triangle is {v: [(x, y, z) * 3], normal: (x, y, z), a: u8 at +0x30, b: u8 at +0x31, id: event id}
    (a / b / id only for stride 0x34; id = u16 at +0x32, but u16 at +0x30 in the examine group +0x30).
    Group header: {triangles, count, cells ({u32 first index, u32 count} per cell), triangle indices (u32),
    u16 cells x, u16 cells z, f32 cell width, f32 cell depth, f32 min x, f32 min z}. The cell size / minimum
    order is from the data (cells of 40 starting exactly at the floor's minimum corner); the C struct
    GSFieldFixedMdlEventList names them the other way round. The loader fixes up only 'triangles' for +0x30 / +0x38."""
    ro, n = struct.unpack(">II", d[:8]); out = []
    for i in range(n):
        r = d[ro + 0x40 * i:ro + 0x40 * (i + 1)]
        f = struct.unpack(">9f", r[:0x24])
        obj = dict(index=i, translate=f[0:3], rotate=f[3:6], scale=f[6:9], flags=struct.unpack(">H", r[0x3C:0x3E])[0], groups={})
        for off, name, stride, _ in GROUPS:
            g = u32(r, off)
            if not g: continue
            data, count = u32(d, g), u32(d, g + 4)
            tris = []
            for k in range(count):
                t = d[data + k * stride:data + (k + 1) * stride]
                if len(t) < stride: break
                v = struct.unpack(">9f", t[:0x24])
                tri = dict(v=[v[0:3], v[3:6], v[6:9]], normal=struct.unpack(">3f", t[0x24:0x30]))
                if stride == 0x34:
                    tri.update(a=t[0x30], b=t[0x31])
                    # event id: examine planes keep it in the u16 at +0x30 (fn_80111864), walk-through triggers at +0x32
                    tri["id"] = struct.unpack(">H", t[0x30:0x32] if off == 0x30 else t[0x32:0x34])[0]
                tris.append(tri)
            grid = None
            if off not in (0x30, 0x38) and u32(d, g + 8):
                cx, cz = struct.unpack(">HH", d[g + 0x10:g + 0x14])
                grid = dict(cells_x=cx, cells_z=cz, cell_size=struct.unpack(">2f", d[g + 0x14:g + 0x1C]), min_xz=struct.unpack(">2f", d[g + 0x1C:g + 0x24]))
            obj["groups"][name] = dict(count=count, triangles=tris, grid=grid)
        out.append(obj)
    return out


def obj_text(objs, title=""):
    """Wavefront OBJ: one 'o' per collision object and group, vertices in room space (translate only;
    rotation / scale are applied when non-trivial, order guessed as scale, rotate X-Y-Z, translate)."""
    import math
    lines = [f"# collision {title}: exported by tools/local/colo/collision.py (docs/engine/collision.md)"]
    vi = 1
    for o in objs:
        sx, sy, sz = o["scale"]; rx, ry, rz = o["rotate"]; tx, ty, tz = o["translate"]
        def xf(p):
            x, y, z = p[0] * sx, p[1] * sy, p[2] * sz
            if rx: y, z = y * math.cos(rx) - z * math.sin(rx), y * math.sin(rx) + z * math.cos(rx)
            if ry: x, z = x * math.cos(ry) + z * math.sin(ry), -x * math.sin(ry) + z * math.cos(ry)
            if rz: x, y = x * math.cos(rz) - y * math.sin(rz), x * math.sin(rz) + y * math.cos(rz)
            return x + tx, y + ty, z + tz
        for name, g in o["groups"].items():
            if not g["triangles"]: continue
            lines.append(f"o obj{o['index']:02d}_{name}" + ("_moving" if o["flags"] & 1 else ""))
            for t in g["triangles"]:
                for p in t["v"]: lines.append("v %.4f %.4f %.4f" % xf(p))
                lines.append(f"f {vi} {vi + 1} {vi + 2}"); vi += 3
    return "\n".join(lines) + "\n"
