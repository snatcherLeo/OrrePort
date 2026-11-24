"""HSD models (map models type 1, object / NPC models type 2, the model inside a PKX): a scene walk from scene_data
through joints, display objects, materials, textures and primitive display lists, to triangles in the model's bind
pose, and an OBJ + MTL export (docs/engine/hsd-models.md). Read-only reader (no file output): a port candidate for
tools (viewer, editors); the port itself renders through the decompiled HSD code.

Layouts: include/hsd/hsd_jobj.h (HSD_Joint), hsd_dobj.h (HSD_DObjDesc), hsd_pobj.h (HSD_PObjDesc, HSD_VtxDescList,
HSD_EnvelopeDesc), hsd_mobj.h (HSD_MObjDesc, material), hsd_tobj.h (HSD_TObjDesc, HSD_ImageDesc), hsd_forward.h
(PObj types). Vertex formats and display-list opcodes are the GameCube GX ones."""
import math
import struct

from .gx import decode_gx, c565, c5a3, cia8
from .gpt1 import GX_TO_GS

GX_VA = {0: "pnmtx", 9: "pos", 10: "nrm", 11: "clr0", 12: "clr1", 13: "tex0", 14: "tex1", 25: "nbt"}
PRIMS = {0x80: "quads", 0x88: "quads2", 0x90: "triangles", 0x98: "strip", 0xA0: "fan", 0xA8: "lines", 0xB0: "linestrip", 0xB8: "points"}


class Archive:
    """An HSD archive (.dat): header, data block, relocation table, root symbols."""
    def __init__(self, dat):
        fsize, dsize, nrel, nroot, nref = struct.unpack_from(">5I", dat, 0)
        self.data = dat[0x20:0x20 + dsize]
        rt = 0x20 + dsize + 4 * nrel; st = rt + 8 * (nroot + nref)
        self.roots = {}
        for i in range(nroot):
            off, so = struct.unpack_from(">II", dat, rt + 8 * i)
            self.roots[dat[st + so:dat.index(b"\0", st + so)].decode("ascii", "replace")] = off
    def u32(self, o): return struct.unpack_from(">I", self.data, o)[0] if 0 <= o <= len(self.data) - 4 else 0
    def f32(self, o): return struct.unpack_from(">f", self.data, o)[0]
    def ptr(self, o):
        v = self.u32(o); return v if 0 < v < len(self.data) else 0


def _mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def srt(s, r, t):
    """HSD joint matrix: translate * rotate (Z * Y * X) * scale."""
    cx, sx, cy, sy, cz, sz = math.cos(r[0]), math.sin(r[0]), math.cos(r[1]), math.sin(r[1]), math.cos(r[2]), math.sin(r[2])
    R = [[cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz],
         [cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz],
         [-sy, sx * cy, cx * cy]]
    return [[R[i][0] * s[0], R[i][1] * s[1], R[i][2] * s[2], t[i]] for i in range(3)] + [[0, 0, 0, 1]]


def _apply(m, v, w=1.0):
    return tuple(m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2] + m[i][3] * w for i in range(3))


def _inv(m):
    """Inverse of an affine 4x4 matrix."""
    a = [r[:3] for r in m[:3]]; t = [m[0][3], m[1][3], m[2][3]]
    det = (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
           + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]))
    if abs(det) < 1e-12: return [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]
    c = [[(a[(j + 1) % 3][(i + 1) % 3] * a[(j + 2) % 3][(i + 2) % 3] - a[(j + 1) % 3][(i + 2) % 3] * a[(j + 2) % 3][(i + 1) % 3]) / det
          for j in range(3)] for i in range(3)]
    return [[c[i][0], c[i][1], c[i][2], -(c[i][0] * t[0] + c[i][1] * t[1] + c[i][2] * t[2])] for i in range(3)] + [[0, 0, 0, 1]]


def _tex_transform(a, td):
    """MakeTextureMtx (src/hsd/tobj.c) for one TObjDesc: uv' = S x R x T x uv with S = repeat / scale, T = -translate
    (t also shifted by 1 / (repeat_t / scale_t) when wrap_t is GX_MIRROR). Returns (su, sv, tu, tv, rotated)."""
    rot = (a.f32(td + 0x10), a.f32(td + 0x14), a.f32(td + 0x18))
    sx, sy = a.f32(td + 0x1C), a.f32(td + 0x20); tx, ty = a.f32(td + 0x28), a.f32(td + 0x2C)
    wrap_t = a.u32(td + 0x38); rs, rt = a.data[td + 0x3C] or 1, a.data[td + 0x3D] or 1
    su = 0.0 if abs(sx) < 1e-7 else rs / sx; sv = 0.0 if abs(sy) < 1e-7 else rt / sy
    mirror = (1.0 / sv) if wrap_t == 2 and sv else 0.0
    return (su, sv, -tx, -(ty + mirror), any(abs(r) > 1e-6 for r in rot))


def apply_uv(uv, t):
    """UV through a material's texture transform (rotation ignored: see hsd-models.md)."""
    if not uv or not t: return uv
    return ((uv[0] + t[2]) * t[0], (uv[1] + t[3]) * t[1])


def _hidden(joints, jt):
    """True when the joint or an ancestor has JOBJ_HIDDEN (0x10): not drawn unless an animation or the game shows it."""
    while jt is not None:
        if jt["flags"] & 0x10: return True
        jt = joints[jt["parent"]] if jt["parent"] is not None else None
    return False


def _envelope_node_mtx(joints, jt):
    """_HSD_mkEnvelopeModelNodeMtx (src/hsd/displayfunc.c) at the bind pose: None for a skeleton root; else, with X the
    nearest skeleton (self or ancestor flagged SKELETON / SKELETON_ROOT): X == owner -> inverse of its inverse-bind
    matrix; X a skeleton root -> inverse(X world) x owner world; otherwise inverse(X world x X inverse-bind) x owner
    world."""
    if jt["flags"] & 2: return None
    x = jt
    while x is not None and not x["flags"] & 3:
        x = joints[x["parent"]] if x["parent"] is not None else None
    if x is None: return None
    if x is jt: return _inv(jt["inv_bind"]) if jt["inv_bind"] else jt["world"]
    if x["flags"] & 2: return _mul(_inv(x["world"]), jt["world"])
    xb = _mul(x["world"], x["inv_bind"]) if x["inv_bind"] else [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]
    return _mul(_inv(xb), jt["world"])


def _read_attr(a, d, idx_or_data, cnt, comp, frac, kind):
    """One attribute value from a vertex array (indexed) or inline."""
    if kind == "clr":
        fmt = {0: (">H", 2), 1: (">3B", 3), 2: (">4B", 4), 3: (">H", 2), 4: (">3B", 3), 5: (">4B", 4)}[comp]
        v = struct.unpack_from(fmt[0], a.data, idx_or_data)
        if comp == 0: c = c565(v[0])
        elif comp == 3: x = v[0]; c = tuple(((x >> s) & 15) * 17 for s in (12, 8, 4, 0))
        elif comp == 4: x = (v[0] << 16) | (v[1] << 8) | v[2]; c = tuple(((x >> s) & 63) * 255 // 63 for s in (18, 12, 6, 0))
        else: c = tuple(v) + ((255,) if len(v) == 3 else ())
        return c
    n = {"pos": 2 + cnt, "nrm": 3 * (1 + 2 * cnt) if cnt else 3, "tex": 1 + cnt}[kind]
    code, size = {0: ("B", 1), 1: ("b", 1), 2: ("H", 2), 3: ("h", 2), 4: ("f", 4)}[comp]
    vals = struct.unpack_from(">" + code * n, a.data, idx_or_data)
    if comp == 4: return vals
    if kind == "nrm": frac = 6 if comp in (1,) else 14 if comp == 3 else frac
    return tuple(v / (1 << frac) for v in vals)


def parse_model(dat):
    """Walk scene_data (or the first root) and return {'joints': [...], 'meshes': [...], 'textures': {img: (w, h, rgba,
    format)}, 'stats': {...}}. A mesh = {joint, material: {diffuse, alpha, texture}, tris: [((pos, nrm, uv, clr) x3)]},
    with positions in the model's bind pose."""
    a = Archive(dat)
    stats = {"pobjs": 0, "envelope": 0, "shapeanim": 0, "skipped_prims": 0}
    roots = []
    if "scene_data" in a.roots:
        sets = a.ptr(a.roots["scene_data"]); k = 0
        while sets and a.ptr(sets + 4 * k):
            js = a.ptr(a.ptr(sets + 4 * k))
            if js: roots.append(js)
            k += 1
    else:
        roots = [o for n, o in a.roots.items() if n.endswith("_joint")]
    joints = []; by_off = {}
    def walk(j, parent_m, depth=0, parent=None):
        while j and depth < 200 and j not in by_off:
            flags = a.u32(j + 4)
            r = (a.f32(j + 0x14), a.f32(j + 0x18), a.f32(j + 0x1C)); s = (a.f32(j + 0x20), a.f32(j + 0x24), a.f32(j + 0x28))
            t = (a.f32(j + 0x2C), a.f32(j + 0x30), a.f32(j + 0x34))
            m = _mul(parent_m, srt(s, r, t))
            ib = a.ptr(j + 0x38)
            inv_bind = [[a.f32(ib + 16 * r + 4 * c) for c in range(4)] for r in range(3)] + [[0, 0, 0, 1]] if ib else None
            by_off[j] = len(joints)
            joints.append({"offset": j, "flags": flags, "world": m, "parent": parent, "inv_bind": inv_bind,
                           "dobj": a.ptr(j + 0x10) if not flags & (JOBJ_SPLINE | JOBJ_PTCL) else 0})
            walk(a.ptr(j + 8), m, depth + 1, by_off[j])
            j = a.ptr(j + 0x0C)
    JOBJ_SPLINE, JOBJ_PTCL = 1 << 14, 1 << 5
    ident = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]
    for r in roots: walk(r, ident)
    for jt in joints: jt["envelope_right"] = _envelope_node_mtx(joints, jt)
    textures = {}; tex_mtx = [None]
    def decode(td):
        """Decode one TObj's image into `textures` (keyed by image offset); returns the offset or None."""
        img = a.ptr(td + 0x4C)
        if not img: return None
        data, w, h, fmt = a.ptr(img), *struct.unpack_from(">HHI", a.data, img + 4)
        if not (data and fmt in GX_TO_GS and 0 < w <= 1024 and 0 < h <= 1024): return None
        if data not in textures:
            pal = None
            if fmt in (8, 9, 10):
                tl = a.ptr(td + 0x50)
                if tl:
                    pd, pf = a.ptr(tl), a.u32(tl + 4); n = struct.unpack_from(">H", a.data, tl + 12)[0]
                    conv = {0: cia8, 1: c565, 2: c5a3}.get(pf, c5a3)
                    pal = [conv(struct.unpack_from(">H", a.data, pd + 2 * i)[0]) for i in range(n)]
            try: textures[data] = (w, h, decode_gx(a.data[data:], GX_TO_GS[fmt], w, h, pal), fmt)
            except Exception: return None
        return data
    def texture(mobj, extra):
        """The first decodable TObj is the diffuse map; the later ones (light / environment / other stages) go to
        `extra` as (image offset, texgen source, blend flags)."""
        td = a.ptr(mobj + 8); first = None; guard = 0
        while td and guard < 16:
            guard += 1; data = decode(td)
            if a.ptr(td + 0x58): stats["tev_override"] = stats.get("tev_override", 0) + 1  # HSD_TObjTevDesc (not applied)
            if data is not None:
                if first is None: first = data; tex_mtx[0] = _tex_transform(a, td); tex_mtx.append(a.u32(td + 0x40))
                elif data != first and all(e[0] != data for e in extra): extra.append((data, a.u32(td + 0x0C), a.u32(td + 0x40)))
            td = a.ptr(td + 4)
        return first
    # texture-animation frames: model set +0x08 = null-terminated list of material-animation trees (HSD_MatAnimJoint
    # {child, next, matanim}); each HSD_MatAnim {next, aobjdesc, texanim, renderanim}; each HSD_TexAnim {next, id,
    # aobjdesc, imagetbl, tluttbl, u16 n_imagetbl, u16 n_tluttbl} (include/hsd/hsd_mobj.h, hsd_tobj.h)
    anim_frames = []
    def decode_desc(img, tl):
        if not img: return None
        data, w, h, fmt = a.ptr(img), *struct.unpack_from(">HHI", a.data, img + 4)
        if not (data and fmt in GX_TO_GS and 0 < w <= 1024 and 0 < h <= 1024): return None
        if data not in textures:
            pal = None
            if fmt in (8, 9, 10) and tl:
                pd, pf = a.ptr(tl), a.u32(tl + 4); n = struct.unpack_from(">H", a.data, tl + 12)[0]
                conv = {0: cia8, 1: c565, 2: c5a3}.get(pf, c5a3)
                pal = [conv(struct.unpack_from(">H", a.data, pd + 2 * i)[0]) for i in range(n)]
            try: textures[data] = (w, h, decode_gx(a.data[data:], GX_TO_GS[fmt], w, h, pal), fmt)
            except Exception: return None
        return data
    def mat_tree(mj, depth=0):
        guard = 0
        while mj and depth < 200 and guard < 4096:
            guard += 1; ma = a.ptr(mj + 8); g2 = 0
            while ma and g2 < 256:
                g2 += 1; ta = a.ptr(ma + 8); g3 = 0
                while ta and g3 < 64:
                    g3 += 1; it, tt = a.ptr(ta + 0x0C), a.ptr(ta + 0x10)
                    n_img, n_tl = struct.unpack_from(">HH", a.data, ta + 0x14)
                    for k in range(n_img):
                        img = a.ptr(it + 4 * k) if it else 0
                        tl = a.ptr(tt + 4 * min(k, n_tl - 1)) if tt and n_tl else 0
                        data = decode_desc(img, tl)
                        if data is not None and data not in anim_frames: anim_frames.append(data)
                    ta = a.ptr(ta)
                ma = a.ptr(ma)
            mat_tree(a.ptr(mj), depth + 1); mj = a.ptr(mj + 4)
    if "scene_data" in a.roots:
        sets = a.ptr(a.roots["scene_data"]); k = 0
        while sets and a.ptr(sets + 4 * k) and k < 64:
            mlist = a.ptr(a.ptr(sets + 4 * k) + 8); j = 0
            while mlist and a.ptr(mlist + 4 * j) and j < 256:
                mat_tree(a.ptr(mlist + 4 * j)); j += 1
            k += 1
    meshes = []
    for ji, jt in enumerate(joints):
        d = jt["dobj"]; guard = 0
        while d and guard < 512:
            guard += 1
            mobj = a.ptr(d + 8); mat = {"diffuse": (204, 204, 204, 255), "alpha": 1.0, "texture": None, "uv": None, "extra": []}
            if mobj:
                m = a.ptr(mobj + 0x0C)
                if m: mat["diffuse"] = tuple(a.data[m + 4:m + 8]); mat["alpha"] = a.f32(m + 0x0C)
                del tex_mtx[1:]; tex_mtx[0] = None; mat["texture"] = texture(mobj, mat["extra"]); mat["uv"] = tex_mtx[0]
                mat["tex_flags"] = tex_mtx[1] if len(tex_mtx) > 1 else None  # first stage's TObj flags (stage_mode)
                if mat["uv"] and mat["uv"][4]: stats["rotated_uv"] = stats.get("rotated_uv", 0) + 1
            p = a.ptr(d + 0x0C)
            while p:
                stats["pobjs"] += 1
                tris = _pobj(a, p, jt, joints, by_off, stats)
                if tris: meshes.append({"joint": ji, "material": mat, "tris": tris, "hidden": _hidden(joints, jt)})
                p = a.ptr(p + 4)
            d = a.ptr(d + 4)
    return {"joints": joints, "meshes": meshes, "textures": textures, "anim_frames": anim_frames, "stats": stats}


def _pobj(a, p, jt, joints, by_off, stats):
    flags = struct.unpack_from(">H", a.data, p + 0x0C)[0]; ndisp = struct.unpack_from(">H", a.data, p + 0x0E)[0]
    disp = a.ptr(p + 0x10); kind = flags & 0x3000
    if kind == 0x1000: stats["shapeanim"] += 1
    envs = []
    if kind == 0x2000:
        stats["envelope"] += 1
        el = a.ptr(p + 0x14); i = 0
        while el and a.ptr(el + 4 * i):
            e = a.ptr(el + 4 * i); parts = []
            while a.ptr(e):
                parts.append((by_off.get(a.ptr(e)), a.f32(e + 4))); e += 8
            envs.append(parts); i += 1
    attrs = []; v = a.ptr(p + 8)
    while v:
        at, typ, cnt, comp = struct.unpack_from(">4i", a.data, v)
        if at == 0xFF: break
        frac = a.data[v + 0x10]; stride = struct.unpack_from(">H", a.data, v + 0x12)[0]
        attrs.append((at, typ, cnt, comp, frac, stride, a.ptr(v + 0x14))); v += 0x18
    if not disp or not attrs: return []
    end = min(disp + ndisp * 32, len(a.data)); q = disp; tris = []  # the last 32-byte chunk can run past the data block
    right = jt["envelope_right"]
    def place(pos, nrm, mi):
        # SetupEnvelopeModelMtx (src/hsd/pobj.c) at the bind pose, where every joint's world x inverse-bind is identity:
        # with a node matrix ('right', the owner is not a skeleton root) every envelope vertex goes through it;
        # otherwise a one-joint envelope uses that joint's world matrix and a blended one is in model space.
        m = None
        if kind == 0x2000:
            if right is not None: m = right
            elif envs and mi is not None and mi // 3 < len(envs):
                parts = envs[mi // 3]
                if len(parts) == 1 and parts[0][1] >= 1.0 and parts[0][0] is not None: m = joints[parts[0][0]]["world"]
        else:
            m = jt["world"]
        if m is None: return pos, nrm
        return _apply(m, pos), (_apply(m, nrm, 0.0) if nrm else None)
    while q + 3 <= end:
        try:
            q, got = _prim(a, q, end, attrs, place, stats)
        except (struct.error, IndexError):
            stats["truncated_lists"] = stats.get("truncated_lists", 0) + 1; break
        if got is None: break
        tris += got
    return tris


def _prim(a, q, end, attrs, place, stats):
    """One primitive of a display list: returns (next offset, triangles) or (q, None) at the list's end."""
    tris = []
    if True:
        op = a.data[q]
        if op == 0: return q, None
        prim = op & 0xF8; n = struct.unpack_from(">H", a.data, q + 1)[0]; q += 3
        verts = []
        for _ in range(n):
            vert = {"pos": (0, 0, 0), "nrm": None, "tex0": None, "clr0": None, "pnmtx": None}
            for at, typ, cnt, comp, frac, stride, base in attrs:
                if at <= 8:  # GX_VA_PNMTXIDX (0) and TEX0..7MTXIDX (1-8): one direct byte each
                    if at == 0: vert["pnmtx"] = a.data[q]
                    q += 1; continue
                if typ == 1:  # direct
                    if at in (11, 12):
                        sz = {0: 2, 1: 3, 2: 4, 3: 2, 4: 3, 5: 4}[comp]; val = _read_attr(a, None, q, cnt, comp, frac, "clr"); q += sz
                    else:
                        kind_ = "pos" if at == 9 else "nrm" if at in (10, 25) else "tex"
                        n_ = {"pos": 2 + cnt, "nrm": 3, "tex": 1 + cnt}[kind_]; sz = n_ * {0: 1, 1: 1, 2: 2, 3: 2, 4: 4}[comp]
                        val = _read_attr(a, None, q, cnt, comp, frac, kind_); q += sz
                else:
                    if typ == 2: idx = a.data[q]; q += 1
                    else: idx = struct.unpack_from(">H", a.data, q)[0]; q += 2
                    if at == 0: vert["pnmtx"] = idx; continue
                    o = base + idx * stride
                    kind_ = "clr" if at in (11, 12) else "pos" if at == 9 else "nrm" if at in (10, 25) else "tex"
                    try: val = _read_attr(a, None, o, cnt, comp, frac, kind_)
                    except (struct.error, KeyError): val = None
                name = GX_VA.get(at)
                if name and val is not None: vert[name] = val
            pos = vert["pos"]; pos = (pos[0], pos[1], pos[2] if len(pos) > 2 else 0.0)
            nrm = vert["nrm"][:3] if vert["nrm"] else None
            pos, nrm = place(pos, nrm, vert["pnmtx"])
            verts.append((pos, nrm, vert["tex0"][:2] if vert["tex0"] else None, vert["clr0"]))
        if prim == 0x90: tris += [tuple(verts[i:i + 3]) for i in range(0, n - 2, 3)]
        elif prim == 0x98: tris += [(verts[i], verts[i + 1], verts[i + 2]) if i % 2 == 0 else (verts[i + 1], verts[i], verts[i + 2]) for i in range(n - 2)]
        elif prim == 0xA0: tris += [(verts[0], verts[i], verts[i + 1]) for i in range(1, n - 1)]
        elif prim in (0x80, 0x88): tris += [t for i in range(0, n - 3, 4) for t in ((verts[i], verts[i + 1], verts[i + 2]), (verts[i], verts[i + 2], verts[i + 3]))]
        else: stats["skipped_prims"] += 1
    return q, tris


TEX_COORD = {0: "uv", 1: "reflection", 2: "highlight", 3: "shadow", 4: "toon", 5: "gradation", 6: "backlight"}
TEX_COLORMAP = {0: "none", 1: "alpha mask", 2: "rgb mask", 3: "blend", 4: "modulate", 5: "replace", 6: "pass", 7: "add",
                8: "sub"}
TEX_ALPHAMAP = {0: "none", 1: "alpha mask", 2: "blend", 3: "modulate", 4: "replace", 5: "pass", 6: "add", 7: "sub"}
TEX_LIGHTMAP = ((4, "diffuse"), (5, "specular"), (6, "ambient"), (7, "ext"), (8, "shadow"))


def stage_mode(flags):
    """How one texture stage combines with the colour so far: the TObjDesc +0x40 flags (include/hsd/hsd_tobj.h, used by
    the HSD TEV setup in tobj.c): bits 0-3 coordinate source, bits 4-8 which lighting channel it maps (diffuse,
    specular, ambient, ext, shadow), bits 16-19 colour operation, bits 20-23 alpha operation, bit 24 bump map."""
    light = "/".join(n for b, n in TEX_LIGHTMAP if flags >> b & 1) or "none"
    s = (f"coord {TEX_COORD.get(flags & 0xF, flags & 0xF)}, lightmap {light}, colour "
         f"{TEX_COLORMAP.get(flags >> 16 & 0xF, flags >> 16 & 0xF)}, alpha {TEX_ALPHAMAP.get(flags >> 20 & 0xF, flags >> 20 & 0xF)}")
    return s + (", bump" if flags >> 24 & 1 else "")


def to_obj(model, name, tex_names, hidden=False, mtl_name=None):
    """OBJ and MTL text for the visible meshes (hidden=True: only the meshes on hidden joints).
    tex_names: {image offset: PNG file name} for the textures written next to it."""
    obj = [f"# {name}: HSD model converted by colo/hsdmodel.py (bind pose)" + ("; meshes on hidden joints" if hidden else ""),
           f"mtllib {mtl_name or name}.mtl"]
    mtl = []; mats = {}; vi = vt = vn = 0
    for mi, mesh in enumerate(model["meshes"]):
        if mesh["hidden"] != hidden: continue
        m = mesh["material"]; key = (m["texture"], m["diffuse"], round(m["alpha"], 3), tuple(m.get("extra", [])), m.get("tex_flags"))
        if key not in mats:
            mats[key] = f"mat{len(mats):03d}"
            r, g, b, _ = m["diffuse"]
            mtl += [f"newmtl {mats[key]}", f"Kd {r / 255:.4f} {g / 255:.4f} {b / 255:.4f}", f"d {max(0.0, min(1.0, m['alpha'])):.4f}"]
            if m["texture"] is not None and m["texture"] in tex_names:
                mtl.append(f"map_Kd {tex_names[m['texture']]}")
                if m.get("tex_flags") is not None: mtl.append(f"# texture stage 1: {stage_mode(m['tex_flags'])}")
            for img, src, flags in m.get("extra", []):  # further texture stages: not part of OBJ / MTL, listed for reference
                if img in tex_names: mtl.append(f"# extra texture stage: {tex_names[img]} (texgen source {src}; {stage_mode(flags)})")
            mtl.append("")
        obj += [f"o mesh{mi:04d}_joint{mesh['joint']}", f"usemtl {mats[key]}"]
        for tri in mesh["tris"]:
            face = []
            for pos, nrm, uv, clr in tri:
                obj.append("v %.5f %.5f %.5f" % pos); vi += 1
                ft = fn = ""
                uv = apply_uv(uv, m.get("uv"))
                if uv: obj.append("vt %.5f %.5f" % (uv[0], 1.0 - uv[1])); vt += 1; ft = str(vt)
                if nrm: obj.append("vn %.5f %.5f %.5f" % nrm); vn += 1; fn = str(vn)
                face.append(f"{vi}/{ft}/{fn}" if fn else (f"{vi}/{ft}" if ft else str(vi)))
            obj.append("f " + " ".join(face))
    frames = [tex_names[f] for f in model.get("anim_frames", []) if f in tex_names]
    if frames and not hidden:  # texture-animation frames (TexAnim image tables): exported as PNG, not part of OBJ / MTL
        mtl.append("# texture animation frames: " + ", ".join(frames))
    return "\n".join(obj) + "\n", "\n".join(mtl) + "\n"
