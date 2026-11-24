"""Room cameras (`.cam`, resource type 12): HSD archives whose public symbol `scene_data` is an HSD scene
descriptor; the loader (floorReadCameraPostFunc) uses its camera entry at +0x04. Layouts follow Melee's sysdolphin
(SceneDesc / SceneCameraDesc in melee/src/melee/sc/types.h, HSD_CObjDesc / HSD_CameraAnim in cobj.h, HSD_WObjDesc
in wobj.h, HSD_AObjDesc / HSD_FObjDesc in aobj.h / fobj.h). docs/engine/floor-resources.md ("Room cameras").
Read-only reader (no file output): decodes the static camera and its animation tracks, keyframes included
(colo/fobj.py)."""
import struct

from . import fobj as fobjmod

PROJECTION = {1: "perspective", 2: "frustum", 3: "orthographic"}
# FObj track types, from Melee's animation callbacks (verified there; same HSD library): camera (cobj.c
# CObjUpdateFunc) and position objects for eye / interest (wobj.c WObjUpdateFunc)
CAMERA_TRACK = {1: "eye x", 2: "eye y", 3: "eye z", 5: "interest x", 6: "interest y", 7: "interest z",
                9: "roll", 10: "fov", 11: "near", 12: "far"}
WOBJ_TRACK = {4: "path (spline position)", 5: "x", 6: "y", 7: "z"}


def u32(d, o): return struct.unpack(">I", d[o:o + 4])[0]
def f32(d, o): return struct.unpack(">f", d[o:o + 4])[0]


def archive(d):
    """HSD archive: (data bytes, {public name: offset}). Offsets are relative to the data block (file + 0x20)."""
    fs, dsize, nrel, npub, next_ = struct.unpack(">5I", d[:20])
    data = d[0x20:0x20 + dsize]
    base = 0x20 + dsize + nrel * 4
    strings = base + (npub + next_) * 8
    pubs = {}
    for i in range(npub):
        o, n = struct.unpack(">II", d[base + 8 * i:base + 8 * i + 8])
        pubs[d[strings + n:d.index(b"\0", strings + n)].decode("ascii", "replace")] = o
    return data, pubs


def wobj(data, o):
    return None if not o else dict(pos=struct.unpack(">3f", data[o + 4:o + 16]))


def fobj_chain(data, o, names):
    out = []
    while o:
        nxt, length, start = u32(data, o), u32(data, o + 4), f32(data, o + 8)
        typ, fv, fs = data[o + 0xC], data[o + 0xD], data[o + 0xE]
        ad = u32(data, o + 0x10)
        keys = fobjmod.decode(data[ad:ad + length], start, fv, fs) if ad else []
        out.append(dict(type=typ, track=names.get(typ, f"type {typ}"), bytes=length, start=start, frac_value=fv,
                        frac_slope=fs, keys=keys))
        o = nxt
    return out


def aobj(data, o, names):
    if not o:
        return None
    return dict(flags=u32(data, o), end_frame=f32(data, o + 4), tracks=fobj_chain(data, u32(data, o + 8), names),
                spline=spline(data, u32(data, o + 0x0C)))


def spline(data, j):
    """The rail of a 'path' track: the AObjDesc's object id (+0x0C) is a joint flagged JOBJ_SPLINE (0x4000) whose +0x10
    is an HSD_Spline (include/hsd/hsd_spline.h): u8 type, s16 control-point count, f32 tension, control points (x, y, z),
    f32 total length, segment lengths, per-segment arc-length polynomials. The path value (0..1) is a fraction of the
    arc length (splArcLengthPoint, src/hsd/hsd_wobj.c). Returns None when there is no such joint."""
    if not j or j + 0x14 > len(data) or not u32(data, j + 4) & 0x4000: return None
    sp = u32(data, j + 0x10)
    if not sp or sp + 0x18 > len(data): return None
    typ = data[sp]; n = struct.unpack(">h", data[sp + 2:sp + 4])[0]; cv = u32(data, sp + 8)
    # stored control points: linear n, Bezier (n - 1) * 3 + 1, B-spline and cardinal n + 2 (a phantom point at each
    # end; the curve runs from point 1 to point n) - from splGetSplinePoint's indexing (src/hsd/spline.c)
    ncv = {0: n, 1: (n - 1) * 3 + 1, 2: n + 2, 3: n + 2}.get(typ, n)
    pts = [struct.unpack(">3f", data[cv + 12 * i:cv + 12 * i + 12]) for i in range(max(0, ncv))] if cv else []
    sl, sp5 = u32(data, sp + 0x10), u32(data, sp + 0x14)
    seg = [f32(data, sl + 4 * i) for i in range(n)] if sl else []
    poly = [[f32(data, sp5 + 20 * k + 4 * i) for i in range(5)] for k in range(max(0, n - 1))] if sp5 else []
    return dict(type=typ, numcv=n, tension=f32(data, sp + 4), length=f32(data, sp + 0x0C), points=pts, seg=seg, poly=poly)


def _spline_point(sp, u):
    """splGetSplinePoint (src/hsd/spline.c, fn_801B2038): u in 0..1 over the whole curve."""
    cp = sp["points"]; n = sp["numcv"]; t = sp["tension"]
    def mix(w, base): return tuple(sum(cp[base + i][a] * w[i] for i in range(4)) for a in range(3))
    if u >= 1.0:
        return {0: lambda: cp[n - 1], 1: lambda: cp[(n - 1) * 3], 2: lambda: _spline_point(dict(sp), 0.999999),
                3: lambda: cp[n]}[sp["type"]]()
    x = u * (n - 1); idx = int(x); u = x - idx; u2 = u * u; u3 = u2 * u
    if sp["type"] == 0: return tuple(cp[idx][a] + u * (cp[idx + 1][a] - cp[idx][a]) for a in range(3))
    if sp["type"] == 1:
        v = 1 - u; return mix((v * v * v, 3 * u * v * v, 3 * u2 * v, u3), idx * 3)
    if sp["type"] == 2:
        v = 1 - u; return mix((v * v * v / 6, (4 + 3 * u3 - 6 * u2) / 6, (3 * (-u3 + u2 + u) + 1) / 6, u3 / 6), idx)
    return mix((t * (-u3 + 2 * u2 - u), (2 - t) * u3 + (t - 3) * u2 + 1, (t - 2) * u3 + (3 - 2 * t) * u2 + t * u,
                t * (u3 - u2)), idx)


def _arc_param(sp, a):
    """splArcLengthGetParameter (fn_801B18D8): arc-length fraction -> curve parameter, by bisection on Simpson's rule
    over each segment's arc-length polynomial."""
    if a <= 0: return 0.0
    if a >= 1: return 1.0
    seg = sp["seg"]; idx = 0
    while idx + 1 < len(seg) and seg[idx + 1] < a: idx += 1
    if sp["type"] == 0 or not sp["poly"]:
        hi = seg[idx + 1] if idx + 1 < len(seg) else 1.0
        r = (a - seg[idx]) / ((hi - seg[idx]) or 1)
        return (r + idx) / (sp["numcv"] - 1.0)
    c = sp["poly"][idx]
    def f(t):
        v = c[0] * t ** 4 + c[1] * t ** 3 + c[2] * t * t + c[3] * t + c[4]
        return max(v, 0.0) ** 0.5
    def simpson(s0, s1):
        dx = (s1 - s0) / 8; return dx * (f(s0) + f(s1) + sum((4 if i % 2 == 0 else 2) * f(s0 + dx * (i - 1)) for i in range(2, 9))) / 3
    rem = sp["length"] * (a - seg[idx]); lo, hi = 0.0, 1.0; r = 0.5
    while abs(lo - hi) >= 1e-5:
        r = (lo + hi) / 2; s = simpson(lo, r)
        if rem < 1e-5 + s: hi = r
        else: lo = r; rem -= s
    return (r + idx) / (sp["numcv"] - 1.0)


def rail_point(sp, a):
    """splArcLengthPoint: the camera position for path value a (0..1 of the rail's arc length)."""
    return _spline_point(sp, _arc_param(sp, a))


def cameras(d):
    """-> list of cameras: {projection, eye, interest, roll, near, far, fov, aspect, anims: [...]}"""
    data, pubs = archive(d)
    sd = pubs.get("scene_data")
    if sd is None:
        return []
    cam_entry = u32(data, sd + 4)
    if not cam_entry:
        return []
    desc, anims = u32(data, cam_entry), u32(data, cam_entry + 4)
    proj = struct.unpack(">H", data[desc + 6:desc + 8])[0]
    cam = dict(projection=PROJECTION.get(proj, proj), eye=wobj(data, u32(data, desc + 0x18)),
               interest=wobj(data, u32(data, desc + 0x1C)), roll=f32(data, desc + 0x20),
               near=f32(data, desc + 0x28), far=f32(data, desc + 0x2C), fov=f32(data, desc + 0x30),
               aspect=f32(data, desc + 0x34), anims=[])
    i = 0
    while anims and u32(data, anims + 4 * i):
        a = u32(data, anims + 4 * i)
        eye_a, int_a = u32(data, a + 4), u32(data, a + 8)
        cam["anims"].append(dict(camera=aobj(data, u32(data, a), CAMERA_TRACK),
                                 eye=aobj(data, u32(data, eye_a), WOBJ_TRACK) if eye_a else None,
                                 interest=aobj(data, u32(data, int_a), WOBJ_TRACK) if int_a else None))
        i += 1
    return [cam]


def summary(cam):
    """One readable text block per camera."""
    def v(p): return "(%.1f, %.1f, %.1f)" % p if p else "-"
    lines = [f"projection {cam['projection']}, fov {cam['fov']:.1f}, aspect {cam['aspect']:.3f}, near {cam['near']:.1f}, far {cam['far']:.1f}, roll {cam['roll']:.2f}",
             f"eye {v(cam['eye'] and cam['eye']['pos'])}, interest {v(cam['interest'] and cam['interest']['pos'])}"]
    for k, a in enumerate(cam["anims"]):
        for part in ("camera", "eye", "interest"):
            x = a[part]
            if x:
                tr = ", ".join(f"{t['track']} ({len(t['keys'])} keys)" for t in x["tracks"]) or "no tracks"
                lines.append(f"animation {k} {part}: {x['end_frame']:.0f} frames; {tr}")
                if x.get("spline"):
                    sp = x["spline"]
                    lines.append(f" {part} rail: spline type {sp['type']}, tension {sp['tension']:g}, length {sp['length']:.1f}, "
                                 f"{len(sp['points'])} stored control points: " + " ".join(v(p) for p in sp["points"]))
                    lines.append(f" {part} rail sampled (path value: position): " + " ".join(
                        f"{k / 10:.1f}:" + v(rail_point(sp, k / 10)) for k in range(11)))
                for t in x["tracks"]:
                    lines.append(f" {part} {t['track']} (values {fobjmod.frac_name(t['frac_value'])}, slopes "
                                 f"{fobjmod.frac_name(t['frac_slope'])}, start {t['start']:g}):")
                    lines.append(fobjmod.text(t["keys"]))
    return "\n".join(lines)
