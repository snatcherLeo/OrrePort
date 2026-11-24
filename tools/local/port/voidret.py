"""Functions written `void` that hand back a value anyway (OrrePort; the project notes 14).

On the GameCube a function that ends in a call, with nothing touching r3 before `blr`, returns that call's result
whatever its C says. When such a function sits in a data table (callbacks: text control codes, script natives, AI
tables, menu callbacks), the dispatcher may use that result; on the PC a `void` function returns nothing. This lists
the candidates: functions referenced from `src/game/data/*.c` tables whose live C definition is `void NAME(...)`, whose
original assembly's last call is followed only by the epilogue (no write to r3), and whose last callee returns a
value (canonical prototype in include/pc/prototypes.h). For each table it names the table, so the dispatcher can be
read to see whether it uses the result. Read-only.

Usage (repo root): python -I tools/local/port/voidret.py
"""
import re
from pathlib import Path

EPILOGUE = re.compile(r"^(lwz r0, 0x[0-9a-f]+\(r1\)|mtlr r0|addi r1, r1, 0x[0-9a-f]+|lmw r\d+, 0x[0-9a-f]+\(r1\)|lwz r(2[0-9]|3[01]), 0x[0-9a-f]+\(r1\)|"
                      r"lfd f\d+, 0x[0-9a-f]+\(r1\)|psq_l f\d+, .*|blr|nop)$")


def canonical_returns():
    t = re.sub(r"/\*.*?\*/", "", Path("include/pc/prototypes.h").read_text(encoding="utf-8"), flags=re.S)
    out = {}
    for m in re.finditer(r"^([A-Za-z_][\w\s\*]*?)\b(\w+)\s*\([^;{}]*\)\s*;", t, re.M):
        out[m.group(2)] = m.group(1).strip()
    return out


def main():
    rets = canonical_returns()
    tables = {}
    for p in sorted(Path("src/game/data").glob("*.c")):
        t = p.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"^\S[^=\n]*\b(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)^\};", t, re.M | re.S):
            for n in re.findall(r"\(\(u8\*\)(\w+)\)", m.group(2)):
                tables.setdefault(n, set()).add(f"{p.name}:{m.group(1)}")
    src = {}
    for p in Path("src/game").rglob("*.c"):
        for m in re.finditer(r"^void\s+(\w+)\s*\(([^)]*)\)\s*\{?\s*$", p.read_text(encoding="utf-8", errors="replace"), re.M):
            src.setdefault(m.group(1), p.as_posix())
    asm = {}
    for p in Path("build/GC6E01/asm").rglob("*.s"):
        t = p.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"^\.fn (\w+), \w+\n(.*?)^\.endfn \1", t, re.M | re.S):
            asm.setdefault(m.group(1), m.group(2))
    hits = 0
    for name in sorted(set(tables) & set(src)):
        a = asm.get(name)
        if not a:
            continue
        ins = [re.sub(r"^/\*.*?\*/\s*", "", l).strip() for l in a.splitlines() if "*/" in l]
        calls = [k for k, i in enumerate(ins) if i.startswith("bl ")]
        if not calls:
            continue
        last = calls[-1]
        tail = ins[last + 1:]
        if not tail or not all(EPILOGUE.match(i) for i in tail):
            continue
        callee = ins[last].split()[1]
        r = rets.get(callee, "")
        if not r or r == "void":
            continue
        hits += 1
        print(f"{name} ({src[name]}): ends in {callee} -> {r}; tables: {', '.join(sorted(tables[name]))}")
    print(f"{hits} candidates")


if __name__ == "__main__":
    main()
