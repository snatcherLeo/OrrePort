"""Every GameCube address embedded in Colosseum's battle move scripts, and whether pcfixed.txt covers it (OrrePort,
option 1 of port-gaps 22, 2026-10-17). Read-only.

The battle scripts are byte code (run by wsRunScript) whose commands carry 4-byte big-endian addresses: jumps to other
script bytes, the variables they read and write, and constant tables. The PC places all of that at the GameCube
addresses (tools/local/port/pcfixed.txt). This tool walks every script reachable from the 214-entry move-effect table
(lbl_80379BFF), the script starts the C code passes to the runner or stores in the script position, and every jump
target found on the way, using the command lengths of the decoder (docs/engine/battle-engine.md, "Move sequences"). Any operand word
inside a command that falls in main.dol's address range counts as an embedded address. Each target is classified by
the section it lands in (symbols.txt).

Usage (repo root):
  python -I tools/local/port/battlescript_addrs.py            list the targets outside the script bytes
  python -I tools/local/port/battlescript_addrs.py --check    exit 1 if a target is not covered by pcfixed.txt
Prints addresses and symbol names only (no disc bytes). Reusable for XD with its own table address and lengths.
"""
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pcdata  # noqa: E402  (same folder: main.dol reader, pcfixed.txt reader)

SYMS = Path("config/GC6E01/symbols.txt")
# command lengths in bytes, by opcode (the Gen 3-style table, then Colosseum's changed and added commands)
GEN3 = ("1,7,1,1,1,1,1,1,1,1,1,2,2,1,1,1,3,3,3,5,5,1,1,1,2,7,2,2,10,10,7,8,9,11,7,2,5,1,2,5,5,11,12,14,14,14,6,6,6,10,"
        "14,6,7,9,6,7,9,3,1,2,1,1,1,1,5,5,7,6,1,7,10,1,4,3,1,1,2,2,3,6,6,3,2,2,3,3,2,1,2,10,5,5,2,1,2,1,2,2,2,2,2,6,7,"
        "1,1,1,2,1,1,1,1,2,2,1,5,2,2,1,3,1,1,1,5,6,1,1,1,1,2,5,5,1,5,1,5,5,1,6,1,1,1,2,1,5,5,1,1,5,1,1,1,5,2,1,1,1,1,5,"
        "1,1,1,5,5,5,5,5,5,1,5,5,1,1,1,5,1,5,5,1,5,1,6,1,1,1,1,1,5,1,5,5,1,1,5,1,1,5,9,1,1,1,1,5,1,1,1,5,5,5,5,5,5,5,6,"
        "5,1,5,5,1,5,5,5,1,5,5,5,5,2,6,1,1,1,1,5,1,5,5,5,1,1,1,1,5,1,5,1,1,1,1")
LENGTHS = [int(x) for x in GEN3.split(",")][:0xF8] + [20] * 6 + [1] * 2
for _op, _n in {0x0A: 2, 0x10: 5, 0x4D: 3, 0x4F: 7, 0x5D: 5, 0x70: 1, 0xAA: 5, 0xF0: 5}.items():
    LENGTHS[_op] = _n
END = {0x3C, 0x3D, 0x3E, 0x3F, 0x28, 0x44, 0xF6, 0xF7}  # commands after which a script does not fall through
EFFECT_TABLE, EFFECTS = 0x80379BFF, 214
SCRIPT_LO, SCRIPT_HI = 0x80370000, 0x803A0000  # where script starts may lie
DOL_LO, DOL_HI = 0x80003100, 0x80496740  # main.dol's address range, .bss included


def rd(a, n):
    return pcdata.dol_bytes(a, n)


def read_syms():
    out = []
    for line in SYMS.read_text(encoding="utf-8").splitlines():
        m = re.match(r"(\w+) = \.(\w+):0x([0-9A-Fa-f]{8});.*?size:0x([0-9A-Fa-f]+)", line)
        if m:
            out.append((int(m.group(3), 16), int(m.group(4), 16), m.group(2), m.group(1)))
    return sorted(out)


def walk():
    """(script command starts, {target address: set of opcodes})"""
    entries = set()
    for i in range(EFFECTS):
        w = rd(EFFECT_TABLE + 4 * i, 4)
        if w:
            entries.add(struct.unpack(">I", w)[0])
    for path in Path("src/game").glob("*.c"):  # script starts the C code hands to the runner or the script position
        text = path.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"wsRunScript\([^;]*?lbl_(80[0-9A-F]{6})", text):
            entries.add(int(m.group(1), 16))
        for m in re.finditer(r"lbl_8047B610\s*=\s*[^;]*?lbl_(80[0-9A-F]{6})", text):
            entries.add(int(m.group(1), 16))
    seen, targets = set(), defaultdict(set)
    todo = sorted(e for e in entries if SCRIPT_LO <= e < SCRIPT_HI)
    while todo:
        a = todo.pop()
        for _ in range(400):
            if a in seen:
                break
            seen.add(a)
            b = rd(a, 1)
            if b is None:
                break
            op = b[0]
            args = rd(a + 1, LENGTHS[op] - 1) or b""
            for k in range(max(0, len(args) - 3)):
                w = struct.unpack(">I", args[k:k + 4])[0]
                if DOL_LO <= w < DOL_HI:
                    targets[w].add(op)
                    if SCRIPT_LO <= w < SCRIPT_HI and rd(w, 1) is not None:
                        todo.append(w)  # likely a jump into script code
            if op in END:
                break
            a += LENGTHS[op]
    return seen, targets


def main():
    syms = read_syms()

    def where(a):
        best = None
        for s, z, sec, name in syms:
            if s > a:
                break
            best = (s, z, sec, name)
        if best is None:
            return "?", "?", None
        s, z, sec, name = best
        return sec, name if a < s + max(z, 1) else name + "+past-end", s

    seen, targets = walk()
    lo_s, hi_s = min(seen), max(seen)
    raws, fvars = pcdata.read_fixed()
    var_spans = {}
    for s, z, _sec, name in syms:
        if name in fvars:
            var_spans[name] = (s, s + max(z, 1))
    print(f"scripts walked: {len(seen)} command starts, script bytes 0x{lo_s:08X}-0x{hi_s:08X}")
    by_place, missing = Counter(), []
    for t in sorted(targets):
        sec, name, _start = where(t)
        in_script = lo_s <= t <= hi_s + 32
        by_place["script" if in_script else sec] += 1
        covered = any(lo <= t < hi for lo, hi in raws) or any(a <= t < b for a, b in var_spans.values())
        if not covered:
            missing.append((t, sec, name))
        if not in_script and "--check" not in sys.argv:
            ops = ",".join(f"0x{o:02X}" for o in sorted(targets[t]))
            print(f"  0x{t:08X} {sec:7} {name:28} commands {ops}{'' if covered else '  NOT COVERED by pcfixed.txt'}")
    print("targets by place:", dict(by_place))
    if missing:
        print(f"NOT COVERED by pcfixed.txt: {len(missing)}")
        for t, sec, name in missing:
            print(f"  0x{t:08X} {sec} {name}")
        sys.exit(1 if "--check" in sys.argv else 0)
    print("every embedded address is covered by pcfixed.txt")


if __name__ == "__main__":
    main()
