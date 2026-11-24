"""WZX move-effect sequences (resource type 16): header, the move's own resource archive, and the timed entries
(values, models, particle banks, screen effects, sounds, markers) as a readable timeline (docs/engine/wzx.md).
Read-only reader (no file output): a port candidate, see docs/tools.md "Readers".

Sources (all src/game): wazaSequenceLoadData, fn_801DC46C (node header), fn_801DC5F0 (sequence header),
_wazaSequenceModelEntryLoad, _wazaSequenceParticleEntryLoad, _wazaSequenceEffectEntryLoad and wazaSequenceEntryLink
(wazaSequence.c); the effect descriptor parser fn_801364A8 and its helpers fn_80137114 / fn_8013735C / fn_8013757C /
fn_80137780 (effect/tracefx.c)."""
import struct

ENTRY_TYPES = {1: "value", 2: "model", 3: "particles", 4: "screen effect", 5: "sound", 6: "control"}  # 6: show / hide the Pokemon, remove its null joint, ... (A, runtime)
# effect kind -> name. verified = start function / strings; the rest from effect_visual.c's header comment (guess)
EFFECT_KINDS = {0: ("colour fade (tints the room models)", "verified on the PC by A"), 1: ("electron", "verified"), 2: ("filter", "guess"),
                3: ("lightning", "guess"), 4: ("trail", "verified"), 5: ("leaf", "verified"), 6: ("env map / model", "guess"),
                7: ("sea / model", "guess"), 8: ("blur ramp", "guess"), 9: ("aura ramp", "guess"), 10: ("distortion", "guess"),
                11: ("patchiru texture", "verified"), 12: ("billboard", "guess")}


def _s(b, o): return struct.unpack_from(">i", b, o)[0]
def _u(b, o): return struct.unpack_from(">I", b, o)[0]
def _f(b, o): return struct.unpack_from(">f", b, o)[0]
def a32(x): return (x + 0x1F) & ~0x1F


def node(b, o):
    """The 0x70-byte node header (fn_801DC46C); size mode +0x68: 1 = 0x6C bytes (no +0x6C word), else 0x70."""
    mode = _s(b, o + 0x68)
    n = {"key": _s(b, o), "type": _s(b, o + 4), "attach": _s(b, o + 8), "position": _s(b, o + 0x0C),
         "link_key": _s(b, o + 0x10), "own_point": _s(b, o + 0x14), "base_point": _s(b, o + 0x18),
         "length_index": _s(b, o + 0x1C), "timing": list(struct.unpack_from(">16i", b, o + 0x20)),
         "flags": _s(b, o + 0x60), "part": _s(b, o + 0x64), "size_mode": mode,
         "bank_link": _s(b, o + 0x6C) if mode == 2 else 0}
    return n, o + (0x6C if mode == 1 else 0x70)


def effect(b, p):
    """Screen-effect descriptor (fn_801364A8): u32 kind, s32 frames, u32, then kind-specific parameters and any embedded
    texture (GStexture file) or model archive. Returns (dict, next offset)."""
    kind, frames = _u(b, p), _s(b, p + 4); q = p + 0x0C
    e = {"kind": kind, "frames": frames, "texture": None, "archive": None, "keys": None}
    def blob(at, size, what):
        e[what] = (at, size); return at + a32(size)
    if kind == 0:  # fn_80137780: count, mode, flags; colour pairs {rgba from, rgba to, s32 frames, pad}
        count, mode = _s(b, q + 4), _s(b, q + 8)
        d = q + (0x0C if mode in (1, 2) else 0x10)
        e["keys"] = [(_u(b, d + 16 * i), _u(b, d + 16 * i + 4), _s(b, d + 16 * i + 8)) for i in range(count)]
        e["params"] = {"use": _s(b, q), "count": count, "mode": mode}
        return e, d + 16 * count
    if kind == 1:
        e["params"] = {"vectors": [struct.unpack_from(">3f", b, q + k) for k in (0, 0x0C, 0x18, 0x24)],
                       "floats": [_f(b, q + 0x30), _f(b, q + 0x34), _f(b, q + 0x38)], "counts": [_s(b, q + 0x3C), _s(b, q + 0x40)],
                       "colour": _u(b, q + 0x44), "value": _s(b, q + 0x48)}
        return e, blob(a32(q + 0x54), _s(b, q + 0x4C), "texture")
    if kind == 2:
        e["params"] = {"colour": _u(b, q), "floats": [_f(b, q + 4), _f(b, q + 8)], "ints": [_s(b, q + 0x0C), _s(b, q + 0x10)], "float": _f(b, q + 0x14)}
        return e, q + 0x1C
    if kind == 3:
        e["params"] = {"colours": [_u(b, q), _u(b, q + 4)], "value": _u(b, q + 8), "vectors": [struct.unpack_from(">3f", b, q + 0x0C), struct.unpack_from(">3f", b, q + 0x18)],
                       "floats": list(struct.unpack_from(">9f", b, q + 0x24)), "count": _u(b, q + 0x48)}
        return e, blob(a32(q + 0x54), _u(b, q + 0x4C), "texture")
    if kind == 4:
        e["params"] = {"colour": _u(b, q), "ints": [_s(b, q + 4), _s(b, q + 8), _s(b, q + 0x0C)]}
        return e, blob(a32(q + 0x18), _s(b, q + 0x10), "texture")
    if kind == 5:
        e["params"] = {"floats": list(struct.unpack_from(">10f", b, q)), "ints": [_s(b, q + 0x28), _s(b, q + 0x2C), _s(b, q + 0x30)]}
        return e, blob(a32(q + 0x3C), _s(b, q + 0x34), "archive")
    if kind == 6:
        mode = _u(b, q + 0x3C)
        e["params"] = {"floats": list(struct.unpack_from(">3f", b, q)), "colour": _u(b, q + 0x0C), "floats2": list(struct.unpack_from(">3f", b, q + 0x10)),
                       "grid": [_u(b, q + 0x1C), _u(b, q + 0x20)], "floats3": list(struct.unpack_from(">5f", b, q + 0x24)), "mode": mode}
        return e, blob(a32(q + (0x40 if mode == 1 else 0x44)), _u(b, q + 0x38), "archive")
    if kind == 7:
        e["params"] = {"floats": list(struct.unpack_from(">3f", b, q)), "colour": _u(b, q + 0x0C), "floats2": list(struct.unpack_from(">3f", b, q + 0x10)),
                       "ints": [_s(b, q + 0x1C), _s(b, q + 0x20)], "floats3": [_f(b, q + 0x24), _f(b, q + 0x28)]}
        return e, blob(a32(q + 0x34), _s(b, q + 0x2C), "archive")
    if kind == 8:  # count at +8, mode at +0x0C; mode 2: keys {f32, f32, s32 frames, pad}
        count, mode = _s(b, q + 8), _s(b, q + 0x0C)
        e["params"] = {"ints": [_s(b, q), _s(b, q + 4)], "count": count, "mode": mode}
        if mode == 2:
            e["keys"] = [(_f(b, q + 0x10 + 16 * i), _f(b, q + 0x14 + 16 * i), _s(b, q + 0x18 + 16 * i)) for i in range(count)]
            return e, q + 0x10 + 16 * count
        raise ValueError(f"effect kind 8 mode {mode}: the game's parser leaves its cursor unset")
    if kind == 9:  # count at +0, mode at +4; mode 3 has a value at +8
        count, mode = _s(b, q), _s(b, q + 4)
        d = q + (0x0C if mode == 3 else 8)
        e["params"] = {"count": count, "mode": mode, "value": _u(b, q + 8) if mode == 3 else None}
        if mode in (2, 3):
            e["keys"] = [(_f(b, d + 16 * i), _f(b, d + 4 + 16 * i), _s(b, d + 8 + 16 * i)) for i in range(count)]
            return e, d + 16 * count
        raise ValueError(f"effect kind 9 mode {mode}: the game's parser leaves its cursor unset")
    if kind == 10:
        e["params"] = {"colour": _u(b, q), "switches": [_s(b, q + k) for k in (4, 8, 0x0C, 0x10, 0x14)], "floats": list(struct.unpack_from(">3f", b, q + 0x18))}
        return e, q + 0x28
    if kind == 11:
        e["params"] = {"ints": [_s(b, q + 8), _s(b, q + 0x0C), _s(b, q + 0x10)]}
        size = _s(b, q)
        if size: return e, blob(a32(q + 0x18), size, "archive")
        return e, a32(q + 0x18)
    if kind == 12:
        mode = _s(b, q + 0x14)
        e["params"] = {"ints": [_s(b, q), _s(b, q + 4)], "floats": [_f(b, q + 8), _f(b, q + 0x0C)], "mode": mode}
        return e, blob(a32(q + (0x18 if mode == 1 else 0x1C)), _s(b, q + 0x10), "texture")
    return e, q  # unknown kind: the game's switch does nothing and returns the cursor after the 0x0C header


def parse_wzx(b):
    """A whole WZX file. Returns {header, resource (offset, size) or None, entries: [ {node fields, type name, payload,
    start} ], end}. 'start' is the entry's start time resolved the way wazaSequenceEntryLink does it: either
    ('entry', key, frames) chained to an earlier entry, or ('point', time point index, frames) relative to the owner
    Pokémon's animation time point (which differs per species)."""
    top, o = node(b, 0)  # a node header with no meaning of its own; the sequence header follows it
    h = struct.unpack_from(">6i", b, o)
    count = h[1] - 1  # wazaSequenceLoadData: word +4 after the first node header = entry count + 1
    seq = {"slot": h[0], "count": count, "flags": h[2], "value": h[3], "mode": h[4], "resource_size": h[5]}
    res = None
    if h[4] == 1: o += 0x10
    elif h[4] == 2: o += 0x14
    else:
        at = a32(o + 0x18); res = (at, h[5]) if h[5] else None
        o = at + (a32(h[5]) if h[5] else 0)
    entries = []; by_key = {}
    for i in range(count):
        n, p = node(b, o)
        t = n["type"]; pay = {}
        if t == 1:
            kind, val = _s(b, p), _s(b, p + 4)
            pay = {"value_kind": kind, "value": _f(b, p + 4) if kind == 2 else val}
            o = p + 0x0C + (val * 8 if kind == 3 else 0)
        elif t == 2:
            f = struct.unpack_from(">7i", b, p); size = _u(b, p + 0x1C)
            pay = {"fields": list(f), "model": (a32(p + 0x24), size)}
            o = a32(p + 0x24) + a32(size)
        elif t == 3:
            fmt = _s(b, p + 0x0C); d = p + 0x10 + (4 if fmt == 3 else 0)
            pay = {"fields": [_s(b, p), _s(b, p + 4)], "format": fmt, "size": _u(b, p + 8)}
            if n["bank_link"]:
                pay["bank_from"] = n["bank_link"]; o = d
            else:
                pay["bank"] = (d, _u(b, p + 8)); o = d + a32(_u(b, p + 8))
        elif t == 4:
            pay, o = effect(b, p)
        elif t == 5:
            k = _s(b, p + 4)
            pay = {"sound": _s(b, p), "kind": k, "value": 0 if k in (1, 2) else _s(b, p + 8)}
            o = p + (8 if k in (1, 2) else 0x0C)
        elif t == 6:
            pay = {"value": _s(b, p)}; o = p + 8
        else:
            raise ValueError(f"entry {i}: type {t} (the game logs an error and stops loading)")
        # start time (wazaSequenceEntryLink): chained to an earlier entry with the link key, else the owner's time point
        link = by_key.get(n["link_key"]) if n["link_key"] else None
        if link is not None:
            st = (link["start"][0], link["start"][1], link["start"][2] + link["timing"][n["base_point"]])
        else:
            st = ("point", n["base_point"], 0)
        st = (st[0], st[1], st[2] - n["timing"][n["own_point"]])
        e = dict(n, index=i, type_name=ENTRY_TYPES.get(t, "?"), payload=pay, start=st)
        entries.append(e)
        if n["key"]: by_key.setdefault(n["key"], e)
    return {"top": top, "header": seq, "resource": res, "entries": entries, "end": o, "size": len(b)}


def _start_text(st):
    if st[0] == "point": return f"time point {st[1]}" + (f" {st[2]:+d}" if st[2] else "")
    return f"? {st}"


def listing(w, name=""):
    """Readable timeline of one parsed WZX file (frames at 60 Hz)."""
    h = w["header"]
    L = [f"# {name}", "", f"{len(w['entries'])} entries; animation slot {h['slot']}, flags {h['flags']:#x}, header mode {h['mode']}"
         + (f"; own resource archive {w['resource'][1]} bytes at {w['resource'][0]:#x}" if w["resource"] else "; no own resource archive"), "",
         "Start = the owner's animation time point (per species, from its PKX slot) plus or minus frames, or chained to another entry.", ""]
    points = {e["start"][1] for e in w["entries"]}
    first = min((e["start"][2] for e in w["entries"]), default=0)
    if len(points) == 1 and w["entries"]:
        L += [f"All entries hang off time point {points.pop()}; 'frame' = frames after the first entry (the game shifts the "
              "earliest start to 0, wazaSequenceLoadData).", ""]
        rel = lambda e: f"frame {e['start'][2] - first:4d} | "
    else:
        rel = lambda e: ""
    for e in sorted(w["entries"], key=lambda e: (e["start"][1], e["start"][2], e["index"])):
        p = e["payload"]; t = e["type"]
        tim = ", ".join(f"[{k}]={v}" for k, v in enumerate(e["timing"]) if v) or "none"
        head = f"{rel(e)}[{_start_text(e['start'])}] entry {e['index']} (key {e['key']}): {e['type_name']}"
        if t == 1: what = f"value kind {p['value_kind']} = {p['value']}"
        elif t == 2: what = f"HSD model {p['model'][1]} bytes, settings {p['fields']}"
        elif t == 3: what = (f"GPT1 particle bank {p['bank'][1]} bytes" if "bank" in p else f"particle bank of entry key {p['bank_from']}") + f", settings {p['fields'][0]}, {p['fields'][1]}, format {p['format']}"
        elif t == 4:
            nm, sure = EFFECT_KINDS.get(p["kind"], (f"kind {p['kind']}", "?"))
            what = f"{nm} ({sure}) for {p['frames']} frames" + (f", texture {p['texture'][1]} bytes" if p["texture"] else "") + (f", model archive {p['archive'][1]} bytes" if p["archive"] else "") + (f", {len(p['keys'])} keys" if p["keys"] else "")
        elif t == 5: what = f"sound {p['sound']:#x} (kind {p['kind']}" + (f", {p['value']}" if p["value"] else "") + ")"
        else: what = f"control {p.get('value')}"
        L.append(f"{head}: {what}")
        L.append(f"    attach {e['attach']}, position {e['position']}, part {e['part']}, flags {e['flags']:#x}, timing {tim}, "
                 f"start rule: link key {e['link_key']}, base index {e['base_point']}, own index {e['own_point']}, length index {e['length_index']}")
    return "\n".join(L)
