"""Script natives: do the C signatures agree with the native table's argument types? (OrrePort; script-vm.md)

The script VM calls a native with its arguments split by the native table's type bytes (12-byte records
{function, u8 type[8]}: 2 = float, other non-zero = integer, 0 = unused). On the PC the dispatcher
(port/src/gs_asm_host.c) passes them in that order, so on Windows x64 a native whose C parameters disagree with its
type bytes (an int where the table says float, or the reverse, or more parameters than typed arguments) receives
wrong values; on the GameCube (separate r / f registers) the same mismatch is harmless whenever the counts per kind
agree. This reads the table from src/game/data/data_8027A500.c (decompiled C, committed) and each native's C
definition (or canonical prototype), and lists disagreements. Read-only.

Usage (repo root): python -I tools/local/port/nativecheck.py
"""
import re
from pathlib import Path

FLOAT = re.compile(r"\b(f32|f64|float|double)\b")


def table():
    t = Path("src/game/data/data_8027A500.c").read_text(encoding="utf-8", errors="replace")
    vals = re.findall(r"\(void\*\)(?:\(\(u8\*\)(\w+)\)|(0x[0-9A-Fa-f]+|0))", t)
    out = []
    for k in range(len(vals) - 2):
        name = vals[k][0]
        w1, w2 = vals[k + 1][1], vals[k + 2][1]
        if name and w1 and w2 and all(x <= 3 for x in int(w1, 16).to_bytes(4, "big") + int(w2, 16).to_bytes(4, "big")):
            b = int(w1, 16).to_bytes(4, "big") + int(w2, 16).to_bytes(4, "big")
            out.append((name, list(b)))
    return out


KNOWN = set()


def signatures():
    sig = {}
    proto = re.sub(r"/\*.*?\*/", "", Path("include/pc/prototypes.h").read_text(encoding="utf-8"), flags=re.S)
    for m in re.finditer(r"^[\w\s\*]*?\b(\w+)\s*\(([^;{}()]*)\)\s*;", proto, re.M):
        sig.setdefault(m.group(1), m.group(2))
    for p in Path("src/game").rglob("*.c*"):
        for m in re.finditer(r"^(?:static\s+)?[\w\s\*]+?\b(\w+)\s*\(([^;{}()]*)\)\s*\{", p.read_text(encoding="utf-8", errors="replace"), re.M):
            sig.setdefault(m.group(1), m.group(2))
    return sig


def kinds(params):
    ps = [x.strip() for x in params.split(",") if x.strip() and x.strip() != "void" and x.strip() != "..."]
    return "".join("f" if FLOAT.search(x) else "i" for x in ps)


def main():
    global KNOWN
    t = Path("src/game/data/data_8027A500.c").read_text(encoding="utf-8", errors="replace")
    i = t.find("std212_gbaIsLinkActive")
    KNOWN = set()
    sig = signatures()
    rows = table()
    bad = 0
    for name, types in rows:
        want = "".join("f" if x == 2 else "i" for x in types if x != 0)
        if name not in sig:
            print(f"?    {name}: table {want or '-'}; no C signature found")
            continue
        have = kinds(sig[name])
        if have != want:
            # harmless on the GameCube when the counts per kind agree, wrong on Windows x64 whenever the order differs
            gc_ok = sorted(have) == sorted(want) or (have.count("f") == want.count("f") and len(have) <= len(want))
            print(f"{'ORDER' if gc_ok else 'DIFF '} {name}: table {want or '-'} vs C {have or '-'} ({sig[name].strip()[:80]})")
            bad += 1
    print(f"{len(rows)} natives read, {bad} disagree")


if __name__ == "__main__":
    main()
