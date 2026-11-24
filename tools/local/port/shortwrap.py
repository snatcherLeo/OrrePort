"""One-line wrappers that drop their parameters or their result (OrrePort; the gs_math bug class, leftover-calls.md).

`void vecMag(void) { PSVECMag(); }` compiles to the same bytes as `f32 vecMag(void* v) { return PSVECMag(v); }`:
the wrapper passes its registers straight through. On the PC the first form passes garbage and loses the result.
This lists one-line definitions in src/game whose inner call passes fewer arguments than the callee's parameter
count (canonical prototype in include/pc/prototypes.h, or the SDK prototype in Aurora's headers), and those that
call a value-returning function without returning its result from a void wrapper. Check each with
tools/local/port/argregs.py and the assembly before changing it. Read-only.

Usage (repo root): python -I tools/local/port/shortwrap.py
"""
import re
from pathlib import Path


def param_counts():
    counts, returns = {}, {}
    texts = [Path("include/pc/prototypes.h").read_text(encoding="utf-8")]
    texts += [p.read_text(encoding="utf-8", errors="replace") for p in Path("extern/aurora/include/dolphin").rglob("*.h")]
    for t in texts:
        for m in re.finditer(r"^([A-Za-z_][\w\s\*]*?)\b(\w+)\s*\(([^;{}()]*)\)\s*;", t, re.M):
            ret, name, params = m.group(1).strip(), m.group(2), m.group(3).strip()
            if name in counts:
                continue
            n = 0 if params in ("", "void") else len([x for x in params.split(",") if x.strip() != "..."])
            counts[name] = n
            returns[name] = ret
    return counts, returns


ONE = re.compile(r"^(?!static)([A-Za-z_][\w\s\*]*?)\b(\w+)\s*\(([^)]*)\)\s*\{\s*(return\s+)?(\w+)\s*\(([^()]*)\)\s*;\s*\}\s*$")


def main():
    counts, returns = param_counts()
    hits = 0
    for p in sorted(Path("src/game").rglob("*.c")):
        if "gs_gfx" in p.name:
            continue
        for i, line in enumerate(p.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            m = ONE.match(line)
            if not m:
                continue
            outer_ret, outer, oparams, ret_kw, inner, iargs = m.groups()
            given = 0 if not iargs.strip() else len(iargs.split(","))
            want = counts.get(inner)
            notes = []
            if want is not None and given < want:
                notes.append(f"passes {given} of {inner}'s {want} arguments")
            iret = returns.get(inner, "")
            if outer_ret.strip() == "void" and not ret_kw and iret and iret not in ("void",) and "void" != iret.split()[-1]:
                notes.append(f"void wrapper drops {inner}'s {iret} result")
            if notes:
                hits += 1
                print(f"{p.as_posix()}:{i}: {outer}: {'; '.join(notes)}")
    print(f"{hits} candidates")


if __name__ == "__main__":
    main()
