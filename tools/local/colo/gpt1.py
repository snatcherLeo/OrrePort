"""GPT1 particle banks (HSD particle system): room / effect particles (resource types 10 and 18), the ambient banks in
PKX files and the banks embedded in WZX move effects (docs/engine/particles.md). Read-only reader (no file output): a
port candidate, see docs/tools.md "Readers".

Sources: gsParticleBankLoad (src/game/field_range_80117E58.c: the GPT1 header), psInitDataBankLocate /
psInitDataBank (src/game/particle.c: command lists, texture groups, id table), HSD_PSCmdList / HSD_PSTexGroup
(include/sysdolphin/baselib/psstructs.h) and the particle byte-code interpreter psInterpretParticle0
(src/game/psinterpret.c: every opcode and its operands)."""
import struct

from .gx import decode_gx, c565, c5a3, cia8

GX_TO_GS = {0: 0x40, 1: 0x42, 2: 0x41, 3: 0x43, 4: 0x44, 5: 0x90, 6: 0x45, 8: 0x00, 9: 0x01, 10: 0x30, 14: 0xB0}
GX_NAMES = {0: "I4", 1: "I8", 2: "IA4", 3: "IA8", 4: "RGB565", 5: "RGB5A3", 6: "RGBA8", 8: "C4", 9: "C8", 10: "C14X2", 14: "CMPR"}
CMD_FIELDS = ("type", "tex_group", "gen_life", "life", "kind", "gravity", "friction", "vx", "vy", "vz", "radius", "angle",
              "random", "size", "param1", "param2", "param3")


def find_banks(b):
    """Offsets of every GPT1 bank inside a file (a WZX can embed several)."""
    out = []; o = b.find(b"GPT1")
    while o >= 0:
        if o + 0x14 <= len(b):
            d, t, n, r = struct.unpack_from(">IIII", b, o + 4)
            if 0 < d < t <= len(b) - o and r <= len(b) - o: out.append(o)
        o = b.find(b"GPT1", o + 4)
    return out


def parse_bank(b, base=0):
    """One GPT1 bank at offset base. Returns {version, first_id, lists: [ {id, offset, header fields, code bytes} ],
    tex_groups: [ {fmt, tlut, width, height, num, palnum, palflag, images: [bytes], palettes: [bytes]} ], ids}."""
    desc, tex, tex_size, ref = struct.unpack_from(">IIII", b, base + 4)
    d = base + desc; t = base + tex
    version = struct.unpack_from(">H", b, d)[0]
    if version == 0:
        first = 0; count = struct.unpack_from(">I", b, d + 4)[0]; table = d + 8
    else:
        first, count = struct.unpack_from(">II", b, d + 4); table = d + 12
    offs = list(struct.unpack_from(f">{count}I", b, table))
    starts = sorted(o for o in offs if o) + [tex - desc]
    lists = []
    for i, o in enumerate(offs):
        if not o: lists.append(None); continue
        h = struct.unpack_from(">HHHHI12f", b, d + o)
        end = next(s for s in starts if s > o)
        e = dict(zip(CMD_FIELDS, h)); e.update(index=i, id=first + i, offset=o, code=b[d + o + 0x3C:d + end])
        lists.append(e)
    ngroups = struct.unpack_from(">I", b, t)[0]
    groups = []
    for k in range(ngroups):
        go = struct.unpack_from(">I", b, t + 4 + 4 * k)[0]
        if not go: groups.append(None); continue
        g = t + go
        num, fmt, tlut, w, h, palnum, palflag = struct.unpack_from(">IIIIIHH", b, g)
        n_entries = num
        if fmt in (8, 9, 10):
            n_entries = num + 1 if palflag & 1 else num + palnum if palnum else num * 2
        tab = list(struct.unpack_from(f">{n_entries}I", b, g + 0x18))
        groups.append({"index": k, "num": num, "fmt": fmt, "tlut": tlut, "width": w, "height": h, "palnum": palnum,
                       "palflag": palflag, "image_offsets": tab[:num], "palette_offsets": tab[num:]})
    ids = list(struct.unpack_from(f">{count}I", b, base + ref)) if ref + 4 * count <= len(b) - base else []
    return {"base": base, "version": version, "first_id": first, "count": count, "lists": lists, "tex_groups": groups,
            "ids": ids, "desc": d, "tex": t, "tex_size": tex_size}


def decode_images(b, bank, group):
    """Decode one texture group's images to [(w, h, rgba bytearray)] (base level only)."""
    t = bank["tex"]; fmt = group["fmt"]; w, h = group["width"], group["height"]
    if fmt not in GX_TO_GS: return []
    palettes = []
    if fmt in (8, 9, 10):
        n = 16 if fmt == 8 else 256 if fmt == 9 else 16384
        conv = {0: cia8, 1: c565, 2: c5a3}.get(group["tlut"], c5a3)
        for po in group["palette_offsets"]:
            if not po: palettes.append(None); continue
            n_here = min(n, (len(b) - (t + po)) // 2)
            palettes.append([conv(v) for v in struct.unpack_from(f">{n_here}H", b, t + po)])
    out = []
    for i, io in enumerate(group["image_offsets"]):
        if not io: out.append(None); continue
        pal = palettes[0] if group["palflag"] & 1 and palettes else palettes[i] if i < len(palettes) else None
        out.append((w, h, decode_gx(b[t + io:], GX_TO_GS[fmt], w, h, pal)))
    return out


def _time(c, p):
    v = c[p]; p += 1
    if v & 0x80: v = ((v & 0x7F) << 8) + c[p]; p += 1
    return v, p


def _f(c, p): return struct.unpack_from(">f", c, p)[0], p + 4


def disassemble(code):
    """The particle byte-code (psInterpretParticle0) as [(offset, text)]. Commands below 0x80 wait (and 0x40-0x7F set
    the texture frame first); the rest act at once. Stops after an end (0xFE / 0xFF) or a jump to the mark (0xFD)."""
    out = []; p = 0; n = len(code)
    def fl(k):
        nonlocal p
        vals = []
        for _ in range(k):
            v, p = _f(code, p); vals.append(f"{v:g}")
        return vals
    try:
        while p < n:
            start = p; cmd = code[p]; p += 1
            if cmd < 0x80:
                t = cmd & 0x1F
                if cmd & 0x20: t = (t << 8) + code[p]; p += 1
                if cmd & 0x40:
                    pose = code[p]; p += 1; out.append((start, f"frame {pose}, wait {t}"))
                else: out.append((start, f"wait {t}"))
                continue
            op = cmd & 0xF8 if cmd & 0xF8 <= 0x98 else cmd & 0xF0 if cmd & 0xF0 in (0xC0, 0xD0) else cmd
            if op in (0x80, 0x88, 0x90, 0x98):
                axes = [a for bit, a in ((1, "x"), (2, "y"), (4, "z")) if cmd & bit]
                v = fl(len(axes))
                name = {0x80: "set position", 0x88: "move by", 0x90: "set velocity", 0x98: "add velocity"}[op]
                out.append((start, f"{name} " + ", ".join(f"{a}={x}" for a, x in zip(axes, v))))
            elif op in (0xC0, 0xD0):
                t, p = _time(code, p)
                ch = [a for bit, a in ((1, "r"), (2, "g"), (4, "b"), (8, "a")) if cmd & bit]
                vals = list(code[p:p + len(ch)]); p += len(ch)
                out.append((start, f"{'primary' if op == 0xC0 else 'environment'} colour over {t}: " + ", ".join(f"{a}={v}" for a, v in zip(ch, vals))))
            elif op == 0xA0:
                t, p = _time(code, p); v = fl(1); out.append((start, f"size {v[0]} over {t}"))
            elif op == 0xAC:
                t, p = _time(code, p); v = fl(2); out.append((start, f"size {v[0]} + random {v[1]} over {t}"))
            elif op == 0xA1: out.append((start, "texture off"))
            elif op == 0xA2: out.append((start, f"gravity {fl(1)[0]}"))
            elif op == 0xA3: out.append((start, f"friction {fl(1)[0]}"))
            elif op in (0xA4, 0xB9, 0xF1, 0xF2, 0xA5):
                i = struct.unpack_from(">H", code, p)[0]; p += 2
                what = {0xA4: "spawn particle", 0xF1: "spawn particle (id table)", 0xB9: "spawn particle with velocity",
                        0xF2: "spawn particle with velocity (id table)", 0xA5: "start generator"}[op]
                out.append((start, f"{what} {i}"))
            elif op in (0xEF, 0xF0):
                i = struct.unpack_from(">H", code, p)[0]; k = code[p + 2]; p += 3
                out.append((start, f"start generator{' (id table)' if op == 0xF0 else ''} {i}, kind bits {k & 7}"))
            elif op == 0xAA:
                i, r = struct.unpack_from(">HH", code, p); p += 4
                out.append((start, f"spawn random particle {i} + 0..{r} (id table)"))
            elif op == 0xA6:
                bs, r = struct.unpack_from(">HH", code, p); p += 4; out.append((start, f"life {bs} + random 0..{r}"))
            elif op == 0xA7: out.append((start, f"die with chance {code[p]}%")); p += 1
            elif op == 0xA8: v = fl(3); out.append((start, f"random offset +-{v[0]}, +-{v[1]}, +-{v[2]}"))
            elif op == 0xA9: out.append((start, f"random direction within {fl(1)[0]}"))
            elif op == 0xF4: v = fl(4); out.append((start, f"random direction around ({v[0]}, {v[1]}, {v[2]}) within {v[3]}"))
            elif op == 0xAB: out.append((start, f"scale velocity {fl(1)[0]}"))
            elif op == 0xBE: v = fl(3); out.append((start, f"scale velocity x {v[0]}, y {v[1]}, z {v[2]}"))
            elif op == 0xBD: v = fl(2); out.append((start, f"speed {v[0]} + random {v[1]}"))
            elif op == 0xB3:
                t, p = _time(code, p); m, a, b2 = code[p:p + 3]; p += 3
                out.append((start, f"alpha compare over {t}: mode {m}, refs {a}, {b2}"))
            elif op == 0xB6:
                t, p = _time(code, p); out.append((start, f"rotate by {fl(1)[0]} over {t}"))
            elif op == 0xED:
                v = fl(2); s = code[p]; p += 1; out.append((start, f"rotate by {v[0]} + random {v[1]} (steps {s})"))
            elif op == 0xF3:
                dr = code[p]; p += 1; v = fl(2); t, p = _time(code, p)
                out.append((start, f"spin {'backward' if dr else 'forward'} {v[0]} with spread {v[1]} over {t}"))
            elif op == 0xB7: out.append((start, f"aim at joint point {code[p]}")); p += 1
            elif op == 0xB8:
                j = code[p]; p += 1; v = fl(2); out.append((start, f"pull toward joint point {j}: force {v[0]}, kill radius {v[1]}"))
            elif op == 0xBF: out.append((start, f"follow joint point {code[p]}")); p += 1
            elif op in (0xBA, 0xBB, 0xE0):
                v = [x - 256 if x > 127 else x for x in code[p:p + 4]]; p += 4
                which = {0xBA: "primary", 0xBB: "environment", 0xE0: "both"}[op]
                out.append((start, f"random {which} colour +-{v[0] * 2}, {v[1] * 2}, {v[2] * 2}, {v[3] * 2}"))
            elif op == 0xE9:
                fl_, st = code[p], code[p + 1]; p += 2; k = bin(fl_ & 15).count("1")
                v = [x - 256 if x > 127 else x for x in code[p:p + k]]; p += k
                out.append((start, f"random colour (flags {fl_:#04x}, steps {st}): {v}"))
            elif op == 0xBC: out.append((start, f"frame {code[p]} + random 0..{code[p + 1]}")); p += 2
            elif op == 0xE3: out.append((start, f"palette {code[p]}")); p += 1
            elif op in (0xE4, 0xE5):
                m = ("off", "on", "toggle", "random")[code[p] & 3]; p += 1
                out.append((start, f"{'flip S' if op == 0xE4 else 'flip T'} {m}"))
            elif op == 0xE8: out.append((start, f"trail {fl(1)[0]}"))
            elif op in (0xEA, 0xEB):
                t, p = _time(code, p); f2 = code[p]; p += 1; vals = []
                if f2 & 1: vals.append(f"rgb={code[p]}"); p += 1
                if f2 & 8: vals.append(f"a={code[p]}"); p += 1
                out.append((start, f"{'material' if op == 0xEA else 'ambient'} colour over {t}: " + ", ".join(vals)))
            elif op == 0xFA: out.append((start, f"loop {code[p]} times")); p += 1
            elif op == 0xFB: out.append((start, "loop end"))
            elif op == 0xFC: out.append((start, "mark"))
            elif op == 0xFD: out.append((start, "jump to mark")); break
            elif op in (0xFE, 0xFF): out.append((start, "end")); break
            else:
                flags = {0xAD: "kind |= 0x80", 0xAE: "billboard off (kind &= ~0x60)", 0xAF: "kind 0x20 (not 0x40)",
                         0xB0: "kind 0x40 (not 0x20)", 0xB1: "kind |= 0x60", 0xB2: "detach from its transform",
                         0xB4: "kind |= 0x200", 0xB5: "kind &= ~0x200", 0xE2: "kind |= 8", 0xE6: "kind |= 0x200000",
                         0xE7: "kind &= ~0x200000", 0xF5: "generator follows transform (0x2000)",
                         0xF6: "generator flag 0x1000", 0xF7: "kind |= 0x10000000"}
                out.append((start, flags.get(op, f"op {cmd:#04x} (no operands)")))
    except (IndexError, struct.error):
        out.append((p, "(truncated)"))
    return out


def listing(b, bank, name=""):
    """Readable text for one bank: header, every command list with its fields and disassembly, texture groups."""
    L = [f"GPT1 bank {name} at {bank['base']:#x}: version {bank['version']:#x}, ids {bank['first_id']}..{bank['first_id'] + bank['count'] - 1}, "
         f"{len(bank['tex_groups'])} texture groups", ""]
    for g in bank["tex_groups"]:
        if g: L.append(f"texture group {g['index']}: {g['num']} x {g['width']}x{g['height']} {GX_NAMES.get(g['fmt'], g['fmt'])}"
                       + (f", {len(g['palette_offsets'])} palette(s)" if g["palette_offsets"] else ""))
    L.append("")
    for e in bank["lists"]:
        if not e: continue
        ref = bank["ids"][e["index"]] if e["index"] < len(bank["ids"]) else None
        L.append(f"## emitter {e['index']} (id {e['id']}" + (f", table id {ref}" if ref is not None and ref != e['id'] else "") + ")")
        L.append(f"type {e['type']:#06x}, texture group {e['tex_group']}, generator life {e['gen_life']}, particle life {e['life']}, kind {e['kind']:#010x}")
        L.append("gravity {gravity:g}, friction {friction:g}, velocity ({vx:g}, {vy:g}, {vz:g}), radius {radius:g}, angle {angle:g}, "
                 "random {random:g}, size {size:g}, params ({param1:g}, {param2:g}, {param3:g})".format(**e))
        for off, text in disassemble(e["code"]): L.append(f"  {off:04x}  {text}")
        L.append("")
    return "\n".join(L)
