"""Find calls that pass float and integer arguments in the GameCube declaration's order where the PC's prototype has
another order (2026-10-21, after the bike ride's backwards fade).

    python -I tools/local/port/argorder_scan.py [--all]

On the GameCube integer arguments go in r3, r4, ... and floats in f1, f2, ... independently, so a file's own
declaration `extern void fadeSet(f32, s32);` and the real `fadeSet(u16 type, f32 duration)` compile to the same call.
On x64 the position decides, and the PC build uses the canonical prototype (the file's declaration sits in
`#ifndef TARGET_PC`), so `fadeSet(2.0f, 3)` became type 2 for 3 s. The fix is a macro from include/pc/argorder.h
(`fadeSetDurationFirst`) or the callee's order under TARGET_PC.

Reads every header under include/ for canonical prototypes (one-line `ret name(params);`), and every .c / .cpp file
under src/ for declarations inside `#ifndef TARGET_PC` blocks. A function is flagged when both have the same number of
parameters and the same number of floats but the floats sit in other positions. For each flagged declaration it counts
the calls in that file (`name(`) that are not already through a wrapper. --all also lists declarations whose
parameter counts differ (a different problem: missing or extra arguments).
"""
import os, re, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
FLOAT = re.compile(r"\b(f32|f64|float|double)\b")
PROTO = re.compile(r"^\s*(?:extern\s+)?(?:static\s+)?(?:inline\s+)?[\w\s\*]+?\b(\w+)\s*\(([^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*;")


def kinds(params):
    params = params.strip()
    if params in ("", "void"):
        return ""
    out, depth, cur = [], 0, ""
    for ch in params:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    if any(p.strip() == "..." for p in out):
        return None
    return "".join("F" if FLOAT.search(p) and "*" not in p and "(" not in p else "I" for p in out)


def canon():
    """include/pc/prototypes.h (force-included in the PC build, so it is the one the PC compiles against) first; other
    headers only for names it doesn't have"""
    table, primary = {}, set()
    first = os.path.join(ROOT, "include", "pc", "prototypes.h")
    for line in open(first, encoding="utf-8", errors="replace"):
        m = PROTO.match(line)
        if m and kinds(m.group(2)) is not None:
            table.setdefault(m.group(1), set()).add(kinds(m.group(2)))
            primary.add(m.group(1))
    for d, _, files in os.walk(os.path.join(ROOT, "include")):
        for fn in files:
            if not fn.endswith((".h", ".hpp")) or os.path.join(d, fn) == first:
                continue
            for line in open(os.path.join(d, fn), encoding="utf-8", errors="replace"):
                m = PROTO.match(line)
                if m and m.group(1) not in primary:
                    k = kinds(m.group(2))
                    if k is not None:
                        table.setdefault(m.group(1), set()).add(k)
    return table


def gc_decls(path):
    """(line, name, kinds) for each declaration inside a #ifndef TARGET_PC block"""
    out, depth_stack = [], []
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    for i, line in enumerate(lines, 1):
        s = line.strip()
        if s.startswith("#if"):
            depth_stack.append("gc" if re.match(r"#ifndef\s+TARGET_PC\b", s) or re.match(r"#if\s+!\s*defined\s*\(?\s*TARGET_PC", s) else "x")
            continue
        if s.startswith("#else") and depth_stack:
            depth_stack[-1] = "x" if depth_stack[-1] == "gc" else depth_stack[-1]
            continue
        if s.startswith("#endif") and depth_stack:
            depth_stack.pop()
            continue
        if depth_stack and depth_stack[-1] == "gc":
            m = PROTO.match(line)
            if m:
                k = kinds(m.group(2))
                if k is not None:
                    out.append((i, m.group(1), k))
    return out, lines


def main():
    table = canon()
    show_all = "--all" in sys.argv
    found = 0
    for d, _, files in os.walk(os.path.join(ROOT, "src")):
        for fn in files:
            if not fn.endswith((".c", ".cpp", ".cp")):
                continue
            path = os.path.join(d, fn)
            decls, lines = gc_decls(path)
            for ln, name, k in decls:
                pcs = table.get(name)
                if not pcs or k in pcs:
                    continue
                for pc in pcs:
                    same_shape = len(pc) == len(k) and pc.count("F") == k.count("F")
                    if not same_shape and not show_all:
                        continue
                    if "F" not in k and "F" not in pc:
                        continue
                    decl_start = re.compile(r"\s*(extern\b|static\b|//|\*|/\*|(?:const\s+)?(?:void|s8|s16|s32|u8|u16|u32|"
                                            r"int|f32|f64|float|double|BOOL|char)\b[\s\*]+" + re.escape(name) + r"\s*\()")
                    calls = sum(1 for t in lines if re.search(r"(?<![\w.>])" + re.escape(name) + r"\s*\(", t)
                                and not decl_start.match(t))
                    rel = os.path.relpath(path, ROOT).replace("\\", "/")
                    print(f"{rel}:{ln}: {name}: GameCube declaration {k or '()'} vs PC {pc or '()'}"
                          f"{'' if same_shape else ' (parameter count differs)'}; {calls} call(s) in the file")
                    found += 1
    print(f"{found} declaration(s) flagged")


if __name__ == "__main__":
    main()
