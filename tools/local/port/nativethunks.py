"""Generate typed call thunks for the script natives (OrrePort; docs/engine/script-vm.md, "Port note").

On the GameCube cms_CallStdTrampoline loads all 8 integer slots into r3-r10 and all 8 float slots into f1-f8 and
calls the native; each native then takes its n-th integer parameter from integer slot n and its m-th float
parameter from float slot m, including slots the script didn't fill this time (they keep the previous call's
values: an original quirk several natives rely on, e.g. cameraSetAnimeRate reads a float its table entry never
supplies). The faithful PC dispatch is therefore by the native's C signature, not by the table's type bytes:
this writes one thunk per native,
    static u32 t_NAME(const u32* i, const f32* f) { return ((u32 (*)(u32, f32, ...))NAME)(i[0], f[0], ...); }
plus a lookup table {native, thunk}, into build-pc/generated/pc_native_thunks.c (regenerable; contains no game data:
names and C types only). Natives with no C signature found fall back to their type bytes. Port code (the project's
dispatcher) looks the native up and calls its thunk with the two argument arrays.

Usage (repo root): python -I tools/local/port/nativethunks.py
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import nativecheck  # noqa: E402

OUT = Path("build-pc/generated/pc_native_thunks.c")
MARK = "/* ORREPORT-" + "GENERATED-FROM-DISC: no; generated from C signatures by tools/local/port/nativethunks.py */"


def param_kinds(params):
    out = []
    for x in [x.strip() for x in params.split(",") if x.strip() and x.strip() not in ("void", "...")]:
        if re.search(r"\b(f64|double)\b", x) and "*" not in x:
            out.append("d")
        elif re.search(r"\b(f32|float)\b", x) and "*" not in x:
            out.append("f")
        elif "*" in x or "[" in x:
            out.append("p")
        else:
            out.append("i")
    return out


def ret_kind(name, src_ret):
    r = src_ret.strip()
    if re.fullmatch(r"(static\s+)?(inline\s+)?void", r):
        return "v"
    if re.search(r"\b(f32|float|f64|double)\b", r) and "*" not in r:
        return "f"
    if "*" in r:
        return "p"
    return "i"


def main():
    rows = nativecheck.table()
    sig = {}
    rets = {}
    # Which signature wins (fixed 2026-10-16): the function's definition first, then include/pc/prototypes.h, then
    # any other declaration. It used to be the first one seen, so a stray `extern void std078_flagSetFalse(void);` in a
    # field_*.c file made the thunk call the flag setter with no argument: the native then read a leftover register (the
    # address of the VM's argument array) as its flag id (Outskirt Stand, run 23). Conflicting parameter counts are
    # listed in build-pc/generated/pc_native_thunks_conflicts.txt for review.
    rank = {}
    cands = {}
    texts = [(1, re.sub(r"/\*.*?\*/", "", Path("include/pc/prototypes.h").read_text(encoding="utf-8"), flags=re.S))]
    texts += [(2, p.read_text(encoding="utf-8", errors="replace")) for p in sorted(Path("src/game").rglob("*.c*"))]
    for prio, t in texts:
        for m in re.finditer(r"^((?:static\s+)?[\w\s\*]*?)\b(\w+)\s*\(([^;{}()]*)\)\s*(?:/\*[^\n]*\*/\s*)?([{;])", t, re.M):
            name = m.group(2)
            if not m.group(1).strip() or "asm" in m.group(1).split():  # GameCube asm bodies aren't compiled on the PC
                continue
            r = 0 if m.group(4) == "{" else prio
            cands.setdefault(name, set()).add(len(param_kinds(m.group(3))))
            if name not in rank or r < rank[name]:
                rank[name] = r
                sig[name] = m.group(3)
                rets[name] = m.group(1)
    table_names = {n for n, _ in rows}
    conflicts = sorted(n for n in table_names if len(cands.get(n, ())) > 1)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    (OUT.parent / "pc_native_thunks_conflicts.txt").write_text(
        "".join(f"{n}: parameter counts {sorted(cands[n])}, used {len(param_kinds(sig[n]))} "
                f"({'definition' if rank[n] == 0 else 'declaration'})\n" for n in conflicts), encoding="utf-8")
    print(f"natives with conflicting declarations: {len(conflicts)} (pc_native_thunks_conflicts.txt)")
    lines = [MARK, "#include <stdint.h>", "typedef unsigned int u32;", "typedef float f32;", ""]
    thunks = []
    seen = set()
    for name, types in rows:
        if name in seen:
            continue
        seen.add(name)
        if name in sig:
            kinds = param_kinds(sig[name])
            rk = ret_kind(name, rets[name])
            src = "C signature"
        else:
            kinds = ["f" if x == 2 else "i" for x in types if x]
            rk = "i"
            src = "type bytes (no C signature found)"
        ni = nf = 0
        ptypes, args = [], []
        for k in kinds:
            if k in ("f", "d"):
                ptypes.append("float" if k == "f" else "double")
                args.append(f"f[{nf}]" if nf < 8 else "0")
                nf += 1
            else:
                ptypes.append("void*" if k == "p" else "u32")
                args.append((f"(void*)(uintptr_t)i[{ni}]" if k == "p" else f"i[{ni}]") if ni < 8 else "0")
                ni += 1
        rt = {"v": "void", "f": "float", "p": "void*", "i": "u32"}[rk]
        fptype = f"{rt} (*)({', '.join(ptypes) or 'void'})"
        call = f"(({fptype}){name})({', '.join(args)})"
        lines.append(f"extern void {name}(); /* {src}: {''.join(kinds) or '-'} -> {rk} */")
        if rk == "v":
            body = f"{call}; return 0;"
        elif rk == "p":
            body = f"return (u32)(uintptr_t){call};"
        elif rk == "f":
            body = f"(void){call}; return 0; /* a float result goes to f1, the VM keeps r3 */"
        else:
            body = f"return (u32){call};"
        lines.append(f"static u32 t_{name}(const u32* i, const f32* f) {{ (void)i; (void)f; {body} }}")
        thunks.append(name)
    lines += ["", "typedef struct PcNativeThunk { void* native; u32 (*thunk)(const u32* i, const f32* f); } PcNativeThunk;",
              f"const PcNativeThunk pc_native_thunks[{len(thunks)}] = {{"]
    lines += [f"    {{ (void*){n}, t_{n} }}," for n in thunks]
    lines += ["};", f"const unsigned pc_native_thunk_count = {len(thunks)};", ""]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"{len(thunks)} thunks -> {OUT}")


if __name__ == "__main__":
    main()
