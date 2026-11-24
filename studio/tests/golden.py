"""Golden tests for Studio's reader core: its output against the Python tools' (tools/local/colo/) on the player's
own disc.

Why: the core is a C++ port of those readers (the Studio review, 2026-11-17: "each proven by golden tests against the
Python tools' output on the player's own disc"). Both read the same disc; this compares what they return. Outputs
(names, numbers, pictures, vertices: the disc's data) are written only under the build folder (git-ignored), never
into the repo.

    python -I studio/tests/golden.py --disc "<image>" --cli build-pc/studio/orrestudio-cli.exe [--out <dir>]

What is compared:
  1. every species record (413 on the US disc): name, National / Hoenn numbers, model id, types, abilities, base
     stats, the face entries; exact.
  2. the Bulbasaur line's mug shots (normal and shiny, both facings) and PC pictures: pixel by pixel. Not exact by
     design: the core decodes with Aurora's rules (what the port shows), gx.py with its own (gx_texture.cpp lists the
     differences). Allowed: each colour channel within 12 (CMPR's middle colours are blended in eighths by Aurora and
     in thirds by gx.py; plain 5-bit channels differ by at most 1), and RGB ignored where both pixels are transparent.
     The largest difference is printed, so a real decoding bug (a wrong tile order, a swapped channel) still fails.
  3. the Bulbasaur line's battle models (bind pose): joint, mesh and texture counts exact; every triangle vertex's
     position and texture coordinate within 0.001.
  4. the Bulbasaur line's idle keyframes, against colo/fobj.py's decoding.
  5. (added 2026-11-19, the Moves tab) every move record with its name and description, the TM / HM machine table,
     every species' level-up, TM / HM and egg lists, and the move-effect lookup (the general table and every model
     with its own versions: archive, entry and the user's animation slot); exact.
Platforms: plain Python 3; the same on every OS (the CLI's name differs: no .exe off Windows).
"""
import argparse
import json
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "local"))
from colo.disc import Disc  # noqa: E402
from colo.gx import decode_texture_file, crop  # noqa: E402
from colo.fsys import fsys_entries  # noqa: E402
from colo.tables import common_rel, table_rows, species_names  # noqa: E402
from colo import hsdmodel  # noqa: E402
from colo.tables import dol_table, find_entry, gsfsys_toc  # noqa: E402

LINE = [1, 2, 3]  # Bulbasaur, Ivysaur, Venusaur (internal numbers = National Dex for 1-251)
PIXEL_TOL = 12
VERT_TOL = 1e-3


def run(cli, disc, *args):
    r = subprocess.run([cli, "--disc", disc, *args], capture_output=True)
    if r.returncode != 0:
        raise RuntimeError(f"{' '.join(args)}: {r.stderr.decode('utf-8', 'replace').strip()}")
    return r.stdout.decode("utf-8")


def read_png(path):
    """Our own CLI's PNGs: stored deflate blocks, filter 0 (png_write.cpp), read back without a library."""
    import zlib
    b = open(path, "rb").read()
    w, h = struct.unpack(">II", b[16:24])
    o, idat = 8, b""
    while o < len(b):
        n = struct.unpack(">I", b[o:o + 4])[0]; t = b[o + 4:o + 8]
        if t == b"IDAT": idat += b[o + 8:o + 8 + n]
        o += 12 + n
    raw = zlib.decompress(idat); row = w * 4; px = bytearray()
    for y in range(h): px += raw[y * (row + 1) + 1:(y + 1) * (row + 1)]
    return w, h, px


def compare_pixels(a, b):
    """(largest channel difference, pixels compared); RGB skipped where both are transparent."""
    worst = 0
    for i in range(0, len(a), 4):
        if a[i + 3] == 0 and b[i + 3] == 0: continue
        worst = max(worst, *(abs(a[i + k] - b[i + k]) for k in range(4)))
    return worst


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--disc", required=True)
    ap.add_argument("--cli", required=True)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()
    args.cli = os.path.abspath(args.cli)  # Windows' CreateProcess doesn't resolve a relative path with slashes
    out =args.out or os.path.join(os.path.dirname(os.path.abspath(args.cli)), "golden")
    os.makedirs(out, exist_ok=True)
    disc = Disc(args.disc)
    fails = 0

    def check(ok, what):
        nonlocal fails
        print(("PASS " if ok else "FAIL ") + what)
        if not ok: fails += 1

    # 1. species
    rel = common_rel(disc)
    names = species_names(disc, rel)
    rows = table_rows(rel, 0x121a04, 0x14a6e0, 0x11C)
    core = [json.loads(l) for l in run(args.cli, args.disc, "dump", "species", "all").splitlines() if l.strip()]
    check(len(core) == len(rows), f"species count: core {len(core)}, Python {len(rows)}")
    bad = []
    for i, e in enumerate(rows):
        py = {"index": i, "name": names[i] if not names[i].startswith("species") else "",
              "nationalDex": struct.unpack(">H", e[0x12:0x14])[0], "hoennFirst": struct.unpack(">H", e[0x14:0x16])[0],
              "pkxDataId": struct.unpack(">I", e[0x2C:0x30])[0], "types": list(e[0x30:0x32]),
              "abilities": list(e[0x32:0x34]), "baseStats": list(struct.unpack(">6H", e[0x84:0x90])),
              "faces": [list(struct.unpack(">BxHI", e[o:o + 8])) for o in (0x10C, 0x114)]}
        c = core[i] if i < len(core) else {}
        for k, v in py.items():
            if c.get(k) != v: bad.append(f"species {i} {k}: core {c.get(k)!r}, Python {v!r}")
    for b in bad[:10]: print("   ", b)
    check(not bad, f"species records: {len(rows)} compared field by field, {len(bad)} differences")
    # categories ("Seed"): the core reads pda_menu's bank early (msg.cpp); the Python tools scan every bank in order
    from colo.msg import all_text, clean
    msgs = all_text(disc)
    badc = [f"species {i}: core {core[i].get('category')!r}, Python {clean(msgs.get(struct.unpack('>I', e[0x1C:0x20])[0], ''))!r}"
            for i, e in enumerate(rows) if i < len(core) and core[i].get("category") != clean(msgs.get(struct.unpack('>I', e[0x1C:0x20])[0], ""))]
    for b in badc[:10]: print("   ", b)
    check(not badc, f"species categories: {len(rows)} compared, {len(badc)} differences")

    # 2. pictures
    faces = table_rows(rel, 0x44, 0x14a65c, 8)
    tex = {}
    for arch in ("poke_face", "poke_body"):
        for k, name, t, ident, load in fsys_entries(disc.file(arch + ".fsys")):
            if t == 9: tex[ident] = load
    for sp in LINE:
        e = rows[sp]
        for shiny, fo in ((0, 0x10C), (1, 0x114)):
            pid, body = struct.unpack(">HI", e[fo + 2:fo + 8])
            rec = faces[pid]; w, h, rgba, gx = decode_texture_file(tex[struct.unpack(">I", rec[4:8])[0]]())
            y = 42 if (rec[0] & 0x80) and h > 42 else 0
            for facing in (0, 1):
                if facing and w <= 42: continue
                p = os.path.join(out, f"mug_{sp:03d}_{shiny}_{facing}.png")
                run(args.cli, args.disc, "dump", "mugshot", str(sp), str(shiny), str(facing), p)
                cw, ch, px = read_png(p)
                ref = crop(rgba, w, 42 * facing, y, 42, 42)
                worst = compare_pixels(px, ref) if (cw, ch) == (42, 42) else 999
                check(worst <= PIXEL_TOL, f"mug shot {names[sp]} {'shiny' if shiny else 'normal'} facing {facing} ({gx}): largest channel difference {worst}")
            bw, bh, brgba, bgx = decode_texture_file(tex[body]())
            p = os.path.join(out, f"pc_{sp:03d}_{shiny}.png")
            run(args.cli, args.disc, "dump", "pc", str(sp), str(shiny), p)
            cw, ch, px = read_png(p)
            worst = compare_pixels(px, brgba) if (cw, ch) == (bw, bh) else 999
            check(worst <= PIXEL_TOL, f"PC picture {names[sp]} {'shiny' if shiny else 'normal'} ({bgx}, {bw}x{bh}): largest channel difference {worst}")

    # 3. models (normal; the shiny one where the line has one)
    toc = gsfsys_toc(disc)
    pkx = dol_table(disc, 0x80370BD0, 0x1390 // 12)
    for sp in LINE:
        pid = struct.unpack(">I", rows[sp][0x2C:0x30])[0]
        a, k, en, load = find_entry(disc, pkx[pid][0], pkx[pid][1], toc)
        data = load(); msize = struct.unpack(">I", data[:4])[0]
        m = hsdmodel.parse_model(data[0x40:0x40 + msize])
        p = os.path.join(out, f"model_{sp:03d}.txt")
        run(args.cli, args.disc, "dump", "model", str(sp), "normal", p)
        lines = open(p).read().splitlines()
        head = lines[0].split()
        cj, cm, ct = int(head[1]), int(head[3]), int(head[5])
        check((cj, cm, ct) == (len(m["joints"]), len(m["meshes"]), len(m["textures"])),
              f"model {names[sp]}: joints / meshes / textures core {cj}/{cm}/{ct}, Python {len(m['joints'])}/{len(m['meshes'])}/{len(m['textures'])}")
        worst, n, li = 0.0, 0, 1
        for mesh in m["meshes"]:
            if li >= len(lines): break
            li += 1  # the core's "mesh" line
            for tri in mesh["tris"]:
                for pos, nrm, uv, clr in tri:
                    uv = hsdmodel.apply_uv(uv, mesh["material"].get("uv")) or (0.0, 0.0)
                    if li >= len(lines): worst = 999; break
                    c = [float(x) for x in lines[li].split()]; li += 1; n += 1
                    worst = max(worst, *(abs(c[j] - pos[j]) for j in range(3)), abs(c[3] - uv[0]), abs(c[4] - uv[1]))
        check(worst <= VERT_TOL and li == len(lines), f"model {names[sp]}: {n} vertices compared, largest difference {worst:.6f}")

    # 4. the idle animation's keyframes: at every frame where colo/fobj.py says a key lands, the core's interpreter (a
    # port of the game's FObj player) must give exactly that key's value. The idle animation is the PKX's slot 0.
    from colo import fobj
    from colo.pkx import parse_pkx
    CHANNEL = {1: 0, 2: 1, 3: 2, 8: 3, 9: 4, 10: 5, 5: 6, 6: 7, 7: 8}  # HSD_A_J_* -> rotation, scale, translation
    for sp in LINE:
        pid = struct.unpack(">I", rows[sp][0x2C:0x30])[0]
        a, k, en, load = find_entry(disc, pkx[pid][0], pkx[pid][1], toc)
        pk = load(); p = parse_pkx(pk)
        idle = next((v for t, v in p["slots"][0]["animations"] if t == 0), 0)
        dat = p["model"]; m = hsdmodel.parse_model(dat); A = hsdmodel.Archive(dat)
        index = {jt["offset"]: i for i, jt in enumerate(m["joints"])}
        sets = A.ptr(A.roots["scene_data"]); ms = A.ptr(sets); aj_root = A.ptr(A.ptr(ms + 4) + 4 * idle)
        expect = {}  # frame -> {(joint, channel): value}
        end = 0.0
        stack = [(A.ptr(ms), aj_root)]
        while stack:
            j, aj = stack.pop()
            if not j or not aj: continue
            ao = A.ptr(aj + 8)
            if ao and j in index:
                end = max(end, A.f32(ao + 4)); f = A.ptr(ao + 8)
                while f:
                    length = A.u32(f + 4); start = A.f32(f + 8); typ, fv, fs = A.data[f + 12], A.data[f + 13], A.data[f + 14]
                    ad = A.ptr(f + 16)
                    if typ in CHANNEL and ad:
                        for key in fobj.decode(A.data[ad:ad + length], start, fv, fs):
                            fr = key["frame"]
                            if key["op"] in ("CON", "LIN", "SPL0", "SPL") and key["value"] is not None and fr == int(fr) and 0 <= fr < end:
                                expect.setdefault(int(fr), {})[(index[j], CHANNEL[typ])] = key["value"]
                    f = A.ptr(f)
            stack.append((A.ptr(j + 0x0C), A.ptr(aj + 4)))
            stack.append((A.ptr(j + 0x08), A.ptr(aj)))
        frames = sorted(expect)
        txt = run(args.cli, args.disc, "dump", "joints", str(sp), "idle", ",".join(str(f) for f in frames) or "0")
        got, fr = {}, None
        head = txt.splitlines()[0].split()
        check(int(head[3]) == idle, f"idle animation {names[sp]}: core {head[3]}, PKX slot 0 says {idle} (of {head[1]})")
        for line in txt.splitlines()[1:]:
            parts = line.split()
            if parts[0] == "frame": fr = int(float(parts[1])); continue
            got[(fr, int(parts[0]))] = [float(x) for x in parts[1:]]
        worst, n = 0.0, 0
        for f, keys in expect.items():
            for (j, c), v in keys.items():
                worst = max(worst, abs(got.get((f, j), [1e9] * 9)[c] - v)); n += 1
        check(worst <= 1e-4, f"idle keyframes {names[sp]}: {n} keys over {len(frames)} frames, largest difference {worst:.6f}")

    # 5. the Moves tab's data (2026-11-19): every move record against extract.py's decoder (moves_decoded.csv), the
    # machine table, every species' TM/HM, egg and level-up lists, and the move-effect lookup (general and per-Pokémon)
    from colo.msg import clean as mclean
    mrows = table_rows(rel, 0x11c7c4, 0x14a6d4, 0x38)
    cm = [json.loads(l) for l in run(args.cli, args.disc, "dump", "moves", "all").splitlines() if l.strip()]
    check(len(cm) == len(mrows), f"move count: core {len(cm)}, Python {len(mrows)}")
    bad = []
    for i, e in enumerate(mrows):
        py = {"name": mclean(msgs.get(struct.unpack(">I", e[0x20:0x24])[0], "")), "type": e[2],
              "power": struct.unpack(">h", e[0x16:0x18])[0], "accuracy": e[4], "pp": e[1],
              "priority": struct.unpack(">b", e[0:1])[0], "effect": struct.unpack(">H", e[0x1A:0x1C])[0], "chance": e[5],
              "target": e[3], "contact": e[6], "wzx": struct.unpack(">I", e[0x30:0x34])[0],
              "description": mclean(msgs.get(struct.unpack(">I", e[0x2C:0x30])[0], ""))}
        c = cm[i] if i < len(cm) else {}
        for k, v in py.items():
            if c.get(k) != v: bad.append(f"move {i} {k}: core {c.get(k)!r}, Python {v!r}")
    for b in bad[:10]: print("   ", b)
    check(not bad, f"move records: {len(mrows)} compared field by field (with names and descriptions), {len(bad)} differences")

    dol = disc.dol()
    doffs = struct.unpack(">18I", dol[0:72]); daddrs = struct.unpack(">18I", dol[72:144]); dsizes = struct.unpack(">18I", dol[144:216])
    def dol_at(addr, n):
        for O, A, S in zip(doffs, daddrs, dsizes):
            if A <= addr < A + S: return dol[O + addr - A:O + addr - A + n]
        return b""
    machines = [struct.unpack(">I", dol_at(0x80368018 + 8 * k + 4, 4))[0] for k in range(58)]
    cmach = [int(x) for x in run(args.cli, args.disc, "dump", "machines", "all").split()]
    check(cmach == machines, f"machine table: 58 TMs / HMs, core {'equal' if cmach == machines else cmach[:6]}")
    bad = []
    for i, e in enumerate(rows):
        c = core[i] if i < len(core) else {}
        lv = [[e[0xBA + 4 * k], struct.unpack(">H", e[0xBC + 4 * k:0xBE + 4 * k])[0]] for k in range(20)]
        py = {"machines": list(e[0x34:0x6E]), "eggMoves": list(struct.unpack(">8H", e[0x74:0x84])),
              "levelMoves": [x for x in lv if x[1]]}
        for k, v in py.items():
            if c.get(k) != v: bad.append(f"species {i} {k}: core {c.get(k)!r}, Python {v!r}")
    for b in bad[:10]: print("   ", b)
    check(not bad, f"learnsets: {len(rows)} species' TM/HM, egg and level-up lists, {len(bad)} differences")

    # the effect lookup, as wazaSequenceGetWazaResource does it (src/game/sequence.c, verified from the code): the
    # part the user plays is 1, or 3 for moves aimed at the user (no part 1 in the general table); the per-Pokémon
    # table (lbl_80373750) is searched only in that part's records (part 1: 1..0x11B, part 3: 0x12C..0x15D), the first
    # record for the model, move and variant 0 ending the search (used only when it names a file); else the general
    # table. The slot is the WZX sequence's +0x70 word. (*Corrected 2026-11-19:* the whole per-Pokémon table was searched
    # before, on both sides of this check.)
    own = {1: {}, 3: {}}
    for part, first, last in ((1, 1, 0x11C), (3, 0x12C, 0x15E)):
        for k in range(first, min(last, 0x16F0 // 0x10)):
            r = dol_at(0x80373750 + 0x10 * k, 0x10)
            key = (struct.unpack(">H", r[2:4])[0], struct.unpack(">H", r[4:6])[0])
            if r[0] == 0 and key not in own[part]:
                own[part][key] = struct.unpack(">II", r[8:16])
    slot_cache = {}
    def expect(move, model):
        r = dol_at(0x8036E150 + 0x1C * move, 0x1C)
        arch, ent = struct.unpack(">II", r[4:12]); part = 1
        if not arch: arch, ent = struct.unpack(">II", r[20:28]); part = 3
        oa, oe = own[part].get((model, move), (0, 0))
        is_own = 1 if oa and oe else 0
        if is_own: arch, ent = oa, oe
        if not arch or arch not in toc: return f"{move} - - - -"
        if (arch, ent) not in slot_cache:
            f = find_entry(disc, arch, ent, toc); w = f[3]() if f else b""
            s = struct.unpack(">I", w[0x70:0x74])[0] if len(w) >= 0x74 else -1
            slot_cache[(arch, ent)] = s if 0 <= s < 17 else -1
        return f"{move} {is_own} {toc[arch]} {ent} {slot_cache[(arch, ent)]}"
    models = sorted({m for p in own.values() for m, _ in p})
    bad, n = [], 0
    for model in [None] + models:
        lines = run(args.cli, args.disc, "dump", "effects", "none" if model is None else str(model)).splitlines()
        for i, line in enumerate(lines, 1):
            want = expect(i, 0xFFFFFFFF if model is None else model); n += 1
            if line.strip() != want: bad.append(f"model {model}: core {line.strip()!r}, Python {want!r}")
    for b in bad[:10]: print("   ", b)
    check(not bad, f"move effects: {n} lookups (the general table and {len(models)} models' own versions), {len(bad)} differences")

    # 6. the Pokémon's own particle banks (2026-11-19, particles step 1): every species whose PKX carries a GPT1 bank,
    # each emitter's header, byte-code size and id-table entry, and every texture image pixel by pixel, against
    # colo/gpt1.py (the docs' reader)
    from colo import gpt1
    pk_tab = dol_table(disc, 0x80370BD0, 0x1390 // 12)
    seen, nb, ne, ni, bad, worst = set(), 0, 0, 0, [], 0
    for sp, e in enumerate(rows):
        pid = struct.unpack(">I", e[0x2C:0x30])[0]
        if not pid or pid >= len(pk_tab) or pid in seen: continue
        seen.add(pid)
        f = find_entry(disc, pk_tab[pid][0], pk_tab[pid][1], toc)
        if not f: continue
        pk = f[3]()
        msize, gsize = struct.unpack(">II", pk[:8])
        if not gsize: continue
        base = 0x40 + ((msize + 0x1F) & ~0x1F)
        # (the rest of the file, as the game has it in memory: Gastly's id table runs past the bank's stated size)
        raw = pk[base:]; bk = gpt1.parse_bank(raw)
        pdir = os.path.join(out, f"ptcl_{sp:03d}"); os.makedirs(pdir, exist_ok=True)
        lines = run(args.cli, args.disc, "dump", "particles", str(sp), pdir).splitlines()
        nb += 1
        head = lines[0].split()
        if int(head[2]) != bk["first_id"] or int(head[4]) != bk["count"]:
            bad.append(f"species {sp}: bank first/count core {head[2]}/{head[4]}, Python {bk['first_id']}/{bk['count']}")
        emit = {int(l.split()[1]): l.split() for l in lines if l.startswith("emitter ")}
        for i, L in enumerate(bk["lists"]):
            ne += 1
            c = emit.get(i)
            if L is None:
                if not c or c[2] != "none": bad.append(f"species {sp} emitter {i}: core {c}, Python none")
                continue
            want = [L["id"], L["type"], L["tex_group"], L["gen_life"], L["life"], L["kind"]]
            got = [int(c[3]), int(c[5]), int(c[7]), int(c[9]), int(c[11]), int(c[13])] if c and len(c) > 13 else None
            fl = [float(x) for x in c[15:27]] if got else []
            pf = [L[k] for k in gpt1.CMD_FIELDS[5:]]
            if got != want or any(abs(a - b) > 1e-5 * max(1.0, abs(b)) for a, b in zip(fl, pf)) or int(c[28]) != len(L["code"]) \
               or int(c[30]) != (bk["ids"][i] if i < len(bk["ids"]) else 0):
                bad.append(f"species {sp} emitter {i}: core {' '.join(c[2:]) if c else None}")
        for g in bk["tex_groups"]:
            if g is None: continue
            for k, im in enumerate(gpt1.decode_images(raw, bk, g)):
                if im is None: continue
                ni += 1
                p = os.path.join(pdir, f"ptcl_{g['index']}_{k}.png")
                if not os.path.exists(p): bad.append(f"species {sp} group {g['index']} image {k}: core wrote none"); continue
                cw, ch, px = read_png(p)
                if (cw, ch) != (im[0], im[1]): worst = 999; continue
                # colour exactly; transparency: the core follows GX (I4 / I8: alpha = intensity, as the particles' soft
                # edges need), gx.py makes them opaque, so for those formats alpha must equal the red channel
                for q in range(0, len(px), 4):
                    worst = max(worst, *(abs(px[q + c] - im[2][q + c]) for c in range(3)))
                    worst = max(worst, abs(px[q + 3] - (px[q] if g["fmt"] in (0, 1) else im[2][q + 3])))
    for b in bad[:10]: print("   ", b)
    # 7. the particle simulation (2026-11-19, particles step 1): live particles after 30 battle frames (60 steps) of
    # animation 0, against the PC port running the game's own code (models-animation.md "All 15 banks"). Only the
    # emitters whose spawn rate doesn't depend on random numbers are compared exactly; the random-rate ones (Gastly,
    # Vaporeon, Articuno, Moltres, Magcargo) wait for a step-by-step log from the game's code with a known seed
    want = {4: 65, 5: 96, 6: 180, 109: 9, 110: 45, 126: 104}
    got = {}
    for sp, n in want.items():
        line = [l for l in run(args.cli, args.disc, "ptclsim", str(sp), "30", "1", "0").splitlines() if l.startswith("frame 30:")]
        got[sp] = int(line[0].split()[2]) if line else -1
    check(got == want, f"particle simulation: live particles after 60 steps, {len(want)} species, core {got}, the game's {want}")

    # (12 species' models; the three shiny models' banks are byte-identical copies: models-animation.md)
    check(nb == 12 and not bad and worst <= PIXEL_TOL,
          f"particle banks: {nb} PKX banks, {ne} emitters, {ni} texture images compared; {len(bad)} differences, largest pixel difference {worst}")

    print(f"\n{'ALL PASSED' if not fails else f'{fails} FAILED'} (outputs in {out}, git-ignored)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
