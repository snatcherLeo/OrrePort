"""Find zero-filled work records that are too small on the PC (OrrePort).

pcdata.py gives a zero-filled data symbol declared `u8 x[N]` exactly N bytes (its GameCube size). If the code uses it
through a struct with native pointers (`extern FadeCameraWork lbl_x;`, `(FadeCameraWork*)lbl_x`), that struct is
bigger on the PC and writes run past the end into the next variable (A, 2026-10-15: the move-sequence pool header).
This computes each struct's GameCube and PC size (natural alignment; pointers 4 / 8 bytes; PTR32_FIELD / DISC_PTR 4;
nested structs it knows; arrays) and reports every generated `u8 x[N]` whose struct use needs more than N bytes on the
PC. Fix: keep the GameCube layout (PTR32_FIELD) when raw offsets also read the record, or name the struct in
pcdata_types.txt (reserved at twice the size).
Usage (repo root): python -I tools/local/port/bssfit.py [pc_data_main.c]   (default build-pc/generated/pc_data_main.c)
"""
import re
import sys
from pathlib import Path

SC = {"u8": 1, "s8": 1, "char": 1, "unsigned char": 1, "signed char": 1, "BOOL": 4, "u16": 2, "s16": 2, "short": 2,
      "unsigned short": 2, "u32": 4, "s32": 4, "int": 4, "unsigned int": 4, "unsigned": 4, "long": 4, "f32": 4,
      "float": 4, "u64": 8, "s64": 8, "f64": 8, "double": 8, "GXColor": 4, "Vec": 12, "GSvec": 12, "Mtx": 48}
srcs = {p: re.sub(r"/\*.*?\*/|//[^\n]*", "", p.read_text(errors="replace"), flags=re.S)
        for p in list(Path("src/game").rglob("*.c")) + list(Path("include").rglob("*.h"))}

bodies = {}
for t in srcs.values():
    for m in re.finditer(r"typedef\s+struct\s+(?:DISC_STRUCT\s+)?(\w+)?\s*\{([^{}]*)\}\s*(\w+)\s*;", t):
        for n in {m.group(1), m.group(3)} - {None}:
            bodies.setdefault(n, m.group(2))

memo = {}


def size_of(name, pc, depth=0):
    """(size, align) of a struct by name, or None if a field is unknown."""
    key = (name, pc)
    if key in memo:
        return memo[key]
    if name in SC:
        return SC[name], min(SC[name], 8)
    if name not in bodies or depth > 6:
        return None
    off, al = 0, 1
    for decl in bodies[name].split(";"):
        d = " ".join(decl.split())
        if not d or d.startswith("#"):
            continue
        n = 1
        for a in re.findall(r"\[\s*(0x[0-9A-Fa-f]+|\d+)\s*\]", d):
            n *= int(a, 0)
        d = re.sub(r"\[[^\]]*\]", "", d)
        if ":" in d:  # bit-field: count it as its base type once (approximate)
            d = d.split(":")[0]
        if d.startswith(("PTR32_FIELD", "DISC_PTR")):
            sz = al_f = 4
        elif "(*" in d or re.search(r"\*\s*\w+$", d):
            sz = al_f = 8 if pc else 4
        else:
            ty = re.sub(r"\s+\w+$", "", d).replace("struct ", "").replace("const ", "").replace("volatile ", "").strip()
            r = size_of(ty, pc, depth + 1)
            if r is None:
                memo[key] = None
                return None
            sz, al_f = r
        while off % al_f:
            off += 1
        off += sz * n
        al = max(al, al_f)
    while off % al:
        off += 1
    memo[key] = (off, al)
    return memo[key]


def main():
    gen = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("build-pc/generated/pc_data_main.c")
    zero = {m.group(1): int(m.group(2)) for m in re.finditer(r"^u8 (lbl_[0-9A-Fa-f]{8})\[(\d+)\] __attribute__\(\(aligned\(\d+\)\)\);",
                                                              gen.read_text(errors="replace"), re.M)}
    seen = set()
    for p, t in srcs.items():
        for sym in zero:
            if sym not in t:
                continue
            for m in re.finditer(r"\bextern\s+(?:struct\s+)?(\w+)\s+" + sym + r"\s*(?:\[\s*(0x[0-9A-Fa-f]+|\d*)\s*\])?\s*;"
                                 r"|\(\s*(?:struct\s+)?(\w+)\s*\*\s*\)\s*\(?\s*&?\s*" + sym + r"\b", t):
                st = m.group(1) or m.group(3)
                cnt = int(m.group(2), 0) if m.group(2) else 1
                if st in SC or st is None:
                    continue
                gc, pcs = size_of(st, False), size_of(st, True)
                if not gc or not pcs or (sym, st) in seen:
                    continue
                seen.add((sym, st))
                need = pcs[0] * cnt
                if zero[sym] == 8 or gc[0] * cnt > zero[sym]:
                    continue  # a pointer variable whose value is cast, or a view spanning the next symbols
                if need > zero[sym]:
                    print(f"{sym}\t{zero[sym]} bytes generated\t{st}{'[%d]' % cnt if cnt > 1 else ''}: GameCube {gc[0] * cnt}, "
                          f"PC {need}\t({p.name})")


if __name__ == "__main__":
    main()
