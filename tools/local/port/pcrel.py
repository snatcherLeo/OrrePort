"""Work out what each room module's setup code does, from its instructions and relocations (OrrePort; port-gaps.md 8).

Every room module (Dolphin REL) has a `_prolog` that runs when the room loads and an `_epilog` when it unloads. In
169 of the 170 modules on the disc these are straight runs of "load an address, store it into a main-program slot"
(prolog) and "store 0 into the slot" (epilog), using only stwu / stw / mr / lis / addi / li / lwz / blr
(verified 2026-10-15 by an instruction census). PowerPC code can't run on the PC, so this tool reads the code together
with the module's relocation records and lists the stores, e.g.

    S1_out_bf prolog: lbl_80479E60 <- module .data+0x0 ; lbl_80479E5C <- module .data+0x28

The PC's Relocate then performs exactly those stores (with PC addresses) instead of running PowerPC code. Anything
outside that instruction set (a loop, a call) makes the module "needs C" (only `debugMenuData` on the USA disc).

Reads module files from the disc extraction (`extracted/**/*.rel`, git-ignored; elsewhere --extracted DIR or
$ORRE_EXTRACTED, see extracted_dir()). Output: a text report (stdout) and,
with --out, `build-pc/generated/pc_rel_stores.txt` (game-derived, never committed).
Usage: python -I tools/local/port/pcrel.py [--only NAME,...] [--out build-pc/generated] [--verify]
    --verify: compare with the C prologs of the modules the build decompiles (src/rel/*/*.c).
"""
import re
import struct
import sys
from pathlib import Path

R_ADDR32, R_ADDR16_LO, R_ADDR16_HI, R_ADDR16_HA, R_REL24 = 1, 4, 5, 6, 10


def section_names(modname, secs):
    """REL section index -> dtk's name. dtk lists a module's sections in the order of its non-empty sections
    (config/GC6E01/<module>/splits.txt "Sections:"); for modules outside the build the same rule is applied: code is
    .text, one data section is .data, two are .rodata then .data (dtk's naming for the built modules)."""
    present = [i for i, s in enumerate(secs) if s["size"]]
    splits = Path("config/GC6E01") / modname / "splits.txt"
    names = []
    if splits.exists():
        t = splits.read_text(encoding="utf-8")
        block = t.split("Sections:", 1)[1].split("\n\n", 1)[0]
        names = [l.split()[0] for l in block.strip().splitlines()]
    if len(names) != len(present):
        datas = [i for i in present if not secs[i]["code"] and secs[i]["off"]]
        dnames = [".data"] if len(datas) == 1 else [".rodata", ".data"] + [f".data{k}" for k in range(2, len(datas))]
        out, k = {}, 0
        for i in present:
            if secs[i]["code"]:
                out[i] = ".text"
            elif secs[i]["off"]:
                out[i] = dnames[k]; k += 1
            else:
                out[i] = ".bss"
        return out
    return dict(zip(present, names))
MARKER = "ORREPORT-" + "GENERATED-FROM-DISC"


def main_symbols():
    out = {}
    for line in Path("config/GC6E01/symbols.txt").read_text(encoding="utf-8").splitlines():
        m = re.match(r"(\S+) = \.\w+:0x([0-9A-Fa-f]+);", line)
        if m:
            out.setdefault(int(m.group(2), 16), m.group(1))
    return out


def parse_rel(d):
    names = "id prev next numSections sectionInfoOffset nameOffset nameSize version bssSize relOffset impOffset impSize".split()
    h = dict(zip(names, struct.unpack(">12I", d[:0x30])))
    h["prologSection"], h["epilogSection"], h["unresolvedSection"], _ = d[0x30:0x34]
    h["prolog"], h["epilog"], h["unresolved"] = struct.unpack(">III", d[0x34:0x40])
    secs = []
    for s in range(h["numSections"]):
        off, size = struct.unpack(">II", d[h["sectionInfoOffset"] + 8 * s:h["sectionInfoOffset"] + 8 * s + 8])
        secs.append(dict(off=off & ~1, size=size, code=bool(off & 1)))
    relocs = {}  # (section, offset) -> (type, target module, target section, addend)
    imp = d[h["impOffset"]:h["impOffset"] + h["impSize"]]
    for k in range(0, len(imp), 8):
        mid, off = struct.unpack(">II", imp[k:k + 8])
        p, sec, pos = off, None, 0
        while True:
            o, t, s, a = struct.unpack(">HBBI", d[p:p + 8])
            p += 8
            if t == 203:
                break
            if t == 202:
                sec, pos = s, 0
                continue
            pos += o
            if t == 201:
                continue
            relocs[(sec, pos)] = (t, mid, s, a)
    return h, secs, relocs


def run(d, h, secs, relocs, which, symnames, snames=None):
    """Symbolically execute the prolog / epilog. Returns (stores, problem)."""
    sec = h[which + "Section"]
    start = h[which]
    base = secs[sec]["off"]
    regs, stores, pc = {}, [], start
    selfid = h["id"]

    def target(mid, s, a):
        if mid == 0:
            name = symnames.get(a)
            return ("main", name if name else f"0x{a:08X}")
        if mid == selfid:
            return ("module", f"{(snames or {}).get(s, f'sec{s}')}+0x{a:X}")
        return ("other module", f"{mid}:{s}+0x{a:X}")

    for _ in range(4096):
        if pc + 4 > secs[sec]["size"]:
            return stores, "ran off the end of the code"
        ins, = struct.unpack(">I", d[base + pc:base + pc + 4])
        op = ins >> 26
        rD, rA, imm = (ins >> 21) & 31, (ins >> 16) & 31, ins & 0xFFFF
        rel = relocs.get((sec, pc + 2))
        if ins == 0x4E800020:  # blr
            return stores, None
        if op == 15:  # lis / addis
            if rA == 0 and rel and rel[0] == R_ADDR16_HA:
                regs[rD] = ("ha", target(*rel[1:]))
            elif rA == 0 and not rel:
                regs[rD] = ("const", (imm << 16) & 0xFFFFFFFF)
            else:
                return stores, f"addis form not handled at +0x{pc:X}"
        elif op == 14:  # addi / li
            if rA == 0:
                regs[rD] = ("const", imm - 0x10000 if imm & 0x8000 else imm)
            elif rel and rel[0] == R_ADDR16_LO and regs.get(rA, (None,))[0] == "ha":
                regs[rD] = ("addr", target(*rel[1:]))
            elif rA == 1 or rA == 31:
                pass  # stack frame arithmetic
            else:
                return stores, f"addi form not handled at +0x{pc:X}"
        elif op == 36:  # stw rS, d(rA)
            rS = rD
            if rA in (1, 31) and not rel:
                pass  # saving a register into the stack frame
            elif rel and rel[0] == R_ADDR16_LO and regs.get(rA, (None,))[0] == "ha":
                slot = target(*rel[1:])
                stores.append((slot, regs.get(rS, ("unknown", None))))
            else:
                return stores, f"store form not handled at +0x{pc:X}"
        elif op == 37:  # stwu (frame)
            if rA != 1:
                return stores, f"stwu not on the stack at +0x{pc:X}"
        elif op == 32:  # lwz (frame restore)
            if rA not in (1, 11, 31):
                return stores, f"load from memory at +0x{pc:X}"
        elif op == 31 and ((ins >> 1) & 0x3FF) == 444:  # mr / or
            regs[(ins >> 16) & 31] = regs.get(rD, ("frame", None))
        else:
            return stores, f"instruction 0x{ins:08X} (opcode {op}) at +0x{pc:X}: needs C"
        pc += 4
    return stores, "too long"


def fmt_store(slot, value):
    kind, what = value
    if kind == "addr":
        v = f"{what[0]} {what[1]}"
    elif kind == "const":
        v = f"{what}"
    else:
        v = f"? ({kind})"
    return f"{slot[1]} <- {v}"


def extracted_dir():
    """The disc extraction: --extracted DIR, else $ORRE_EXTRACTED, else ./extracted, else the main checkout's
    (../pkmn_colosseum/extracted: the A / B worktrees have none of their own; read only)."""
    import os
    if "--extracted" in sys.argv:
        return Path(sys.argv[sys.argv.index("--extracted") + 1])
    if os.environ.get("ORRE_EXTRACTED"):
        return Path(os.environ["ORRE_EXTRACTED"])
    for d in (Path("extracted"), Path("../pkmn_colosseum/extracted")):
        if d.is_dir():
            return d
    return Path("extracted")


def analyse(only=None):
    symnames = main_symbols()
    rels = {p.stem: p for p in sorted(extracted_dir().rglob("*.rel"))}
    results = {}
    for name, p in sorted(rels.items()):
        if only and name not in only:
            continue
        d = p.read_bytes()
        h, secs, relocs = parse_rel(d)
        res = {"id": h["id"]}
        modname = name.split("_", 1)[1] if name[:3] in ("00_", "01_") else name
        snames = section_names(modname, secs)
        for which in ("prolog", "epilog"):
            res[which] = run(d, h, secs, relocs, which, symnames, snames)
        results[name] = res
    return results


def c_prolog_stores(path):
    """slot <- value pairs from a decompiled module's C _prolog / _epilog."""
    t = Path(path).read_text(errors="replace")
    out = {}
    for which in ("_prolog", "_epilog"):
        m = re.search(r"void\s+" + which + r"\s*\(void\)\s*\{(.*?)\n\}", t, re.S)
        pairs = []
        if m:
            for s, v in re.findall(r"(\w+)\s*=\s*([^;]+);", m.group(1)):
                pairs.append((s, v.strip()))
        out[which[1:]] = pairs
    return out


def verify(results):
    ok = bad = 0
    for cfile in sorted(Path("src/rel").glob("*/*.c")):
        mod = cfile.parent.name
        name = next((n for n in results if n == mod), None) or next((n for n in results if n.endswith("_" + mod)), None)
        if name is None:
            continue
        csto = c_prolog_stores(cfile)
        if not csto["prolog"] and not csto["epilog"]:
            continue  # a module split over several files: only one defines _prolog / _epilog
        res = results[name]
        mid = res["id"]
        for which in ("prolog", "epilog"):
            stores, problem = res[which]
            got = []
            for slot, val in stores:
                if val[0] == "addr" and val[1][0] == "module":
                    secname, off = val[1][1].split("+0x")
                    v = f"lbl_{mid}_{secname.lstrip('.')}_{int(off, 16):X}"
                elif val[0] == "const":
                    v = str(val[1])
                else:
                    v = "?"
                got.append((slot[1], v))
            want = [(s, re.sub(r"^[&(]*\s*(\(\w+\s*\*?\)\s*)?&?", "", v).strip(")").strip()) for s, v in csto[which]]
            want = [(s, "0" if v in ("0", "NULL", "(void*)0") else v) for s, v in want]
            if problem is None and sorted(got) == sorted(want):
                ok += 1
            else:
                bad += 1
                print(f"MISMATCH {mod} {which}: problem={problem}\n  analyser: {got}\n  C:        {want}")
    print(f"verify: {ok} prolog / epilog lists match the decompiled C, {bad} differ")


if __name__ == "__main__":
    only = set(sys.argv[sys.argv.index("--only") + 1].split(",")) if "--only" in sys.argv else None
    results = analyse(only)
    needs_c = [n for n, r in results.items() if r["prolog"][1] or r["epilog"][1]]
    total = sum(len(r["prolog"][0]) for r in results.values())
    lines = []
    for n, r in results.items():
        for which in ("prolog", "epilog"):
            stores, problem = r[which]
            lines.append(f"{n} {which}: " + ("; ".join(fmt_store(s, v) for s, v in stores) or "(nothing)") +
                         (f"  [NEEDS C: {problem}]" if problem else ""))
    print(f"{len(results)} modules analysed; {total} prolog stores; needs C: {', '.join(needs_c) or 'none'}")
    if "--out" in sys.argv:
        out = Path(sys.argv[sys.argv.index("--out") + 1])
        out.mkdir(parents=True, exist_ok=True)
        (out / "pc_rel_stores.txt").write_text(f"# {MARKER}: room-module setup stores, from the disc's modules\n" +
                                               "\n".join(lines) + "\n", encoding="utf-8")
    if "--show" in sys.argv:
        print("\n".join(lines[:12]))
    if "--verify" in sys.argv:
        verify(results)
