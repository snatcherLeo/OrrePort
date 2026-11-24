"""Upstream merge gate, data step (2026-10-18; snatcherLeo: "a permanent, automatic step of the upstream merge gate").

    python -I tools/local/port/merge_datacheck.py <before: build-pc/generated/pc_data_main.c> <after: same file>
                                                   [--sources <before rev> <after rev>]

Why: the PC build takes most game variables from pcdata.py, which generates them with PC types (a pointer is 8 bytes).
When upstream carves new data units, their C now defines some of those variables with GameCube sizes (`u8 x[0x4]`
holding a pointer); C wins over generated data, so on the PC a pointer loses its upper half and memory is corrupted
silently (2026-10-18: the heap array pointer, the people table, the current room...). A u8[] view left for the PC
(pcrawdata.py's `#else extern u8 x[];`) has the same effect on small variables: pcdata.py then sizes them as bytes.

What it checks:
1. every variable pcdata.py generated before the merge: still generated, with the same size and type after it
   (a variable now defined by the project or generated smaller is listed);
2. with --sources: every variable defined by a data unit that is new or changed between the two revisions and that the
   PC generated before (the units to wrap with pcrawdata.py, --bare for small .bss / .sbss / .sdata variables).
Exit status 1 when anything is listed. Run it after the merged PC build's data step, before any boot test; fix with
pcrawdata.py (--bare / --scalars) and rerun until it prints nothing.
"""
import re
import subprocess
import sys
from pathlib import Path

DEF = re.compile(r"^(?!extern|typedef)(?:static\s+)?(?:const\s+)?([A-Za-z_][\w\s\*]*?)\s*\b([A-Za-z_]\w*)\s*"
                 r"(\[[^\]]*\])?\s*(?:__attribute__\(\([^()]*(?:\([^()]*\))?[^()]*\)\))?\s*(?:=|;)")


def generated(path):
    out = {}
    for line in open(path, encoding="utf-8", errors="replace"):
        m = DEF.match(line)
        if m:
            out[m.group(2)] = (" ".join(m.group(1).split()), (m.group(3) or "").strip("[]"))
    return out


def unit_defs(rev, path):
    text = subprocess.run(["git", "show", f"{rev}:{path}"], capture_output=True, text=True, errors="replace").stdout
    names, depth = set(), 0
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("#ifndef TARGET_PC"):
            depth += 1
            continue
        if depth and s.startswith("#endif"):
            depth -= 1
            continue
        if depth:
            continue
        m = DEF.match(line)
        if m:
            names.add(m.group(2))
    return names


def symbols_at(rev):
    text = subprocess.run(["git", "show", f"{rev}:config/GC6E01/symbols.txt"], capture_output=True, text=True).stdout
    out = {}
    for m in re.finditer(r"^(\S+) = (\.\w+):0x([0-9A-Fa-f]+); // type:object(?: size:0x([0-9A-Fa-f]+))?", text, re.M):
        out[m.group(1)] = (m.group(2), int(m.group(3), 16), int(m.group(4) or "0", 16))
    return out


def write_blocks(r0, r1, path):
    """pcblocks.txt: zero-filled (.bss / .sbss) symbols that r1 splits (smaller, with new symbols inside the old range):
    the PC keeps them whole at the r0 size (pcdata.py). Earlier entries are kept; new ones appended."""
    old, new = symbols_at(r0), symbols_at(r1)
    by_sec = {}
    for n, (s, a, z) in new.items():
        by_sec.setdefault(s, []).append((a, n))
    keep = {}
    if Path(path).exists():
        for line in Path(path).read_text(encoding="utf-8").splitlines():
            w = line.split("#")[0].split()
            if len(w) == 2:
                keep[w[0]] = line
    added = []
    for n, (s, a, z) in sorted(old.items(), key=lambda kv: kv[1][1]):
        if s not in (".bss", ".sbss") or n not in new or new[n][2] >= z or n in keep:
            continue
        inner = [m for aa, m in by_sec.get(s, []) if a < aa < a + z]
        if inner:
            added.append(f"{n} 0x{z:X}  # {r0[:9]}: one 0x{z:X} block; {r1[:9]}: 0x{new[n][2]:X} + {len(inner)} labels")
    head = ["# Zero-filled work areas the PC keeps as one block (pcdata.py; written by merge_datacheck.py --write-blocks).",
            "# Addresses and names only (from symbols.txt), no game data. NAME SIZE  # where it comes from"]
    body = [l for l in keep.values()] + added
    Path(path).write_text("\n".join(head + body) + "\n", encoding="utf-8")
    print(f"pcblocks: {len(added)} new blocks, {len(keep)} kept -> {path}")


def main():
    if "--write-blocks" in sys.argv:
        i = sys.argv.index("--write-blocks")
        write_blocks(sys.argv[i + 1], sys.argv[i + 2], sys.argv[i + 3])
        return 0
    a, b = generated(sys.argv[1]), generated(sys.argv[2])
    # names before the merge -> names after it (the rename map, by address), so a renamed variable isn't "missing"
    if "--renames" in sys.argv:
        ren = {}
        for line in open(sys.argv[sys.argv.index("--renames") + 1], encoding="utf-8"):
            if line.startswith(("#", "old\t")) or "\t" not in line:
                continue
            old, new = line.split("\t")[:2]
            ren[old] = new
        a = {ren.get(n, n): v for n, v in a.items()}
    print(f"generated variables: before {len(a)}, after {len(b)}")
    if min(len(a), len(b)) < 1000:  # a real pc_data_main.c has thousands: the parser failed, so nothing was checked
        print("REFUSED: too few variables parsed (parser out of date with pcdata.py's output?)")
        return 2
    bad = 0
    for n, (ty, size) in sorted(a.items()):
        if n not in b:
            print(f"NOT GENERATED NOW  {n}: was {ty} [{size}] (C defines it now? check its unit)")
            bad += 1
        elif b[n] != (ty, size):
            print(f"CHANGED            {n}: {ty} [{size}] -> {b[n][0]} [{b[n][1]}]")
            bad += 1
    if "--sources" in sys.argv:
        r0, r1 = sys.argv[sys.argv.index("--sources") + 1:sys.argv.index("--sources") + 3]
        files = subprocess.run(["git", "diff", "--name-only", r0, r1, "--", "src/game/data", "src/dolphin"],
                               capture_output=True, text=True).stdout.split()
        for f in files:
            if not f.endswith(".c"):
                continue
            for n in sorted(unit_defs(r1, f) & set(a)):
                print(f"C DEFINES          {n} in {f} (the PC generated it before: wrap it, pcrawdata.py)")
                bad += 1
    print(f"{bad} data mismatches" + ("" if bad else ": the PC's data is as before the merge"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
