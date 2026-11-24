"""Find game calls that pass fewer arguments than the Dolphin SDK function really takes (OrrePort; A's finding).

Game files often declare SDK functions locally with short parameter lists (`extern void PSMTXRotRad(void*, u32);`,
`extern void C_MTXOrtho(void);`), so neither MWCC nor GCC sees that a call drops arguments: on the GameCube the
missing values are still in their registers (f1-f4, r5...), on the PC the callee reads garbage. This tool takes each
SDK function's real parameter count from its definition in src/dolphin (or its prototype in include/dolphin) and
lists every call in src/game, src/hsd and src/musyx with fewer arguments. Read-only.

Usage (repo root): python -I tools/local/port/sdkarity.py [--all]   (--all also lists calls inside `#if 0` copies)
"""
import re
import sys
from pathlib import Path

DEF = re.compile(r"^(?:asm\s+)?(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*(?:\{|$)", re.M)


def count_params(params):
    params = re.sub(r"/\*.*?\*/", "", params).strip()
    if params in ("", "void"):
        return 0, False
    parts = split_args(params)
    variadic = parts[-1].strip() == "..."
    return len(parts) - (1 if variadic else 0), variadic


def split_args(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    return out


def sdk_arities():
    ar = {}
    for p in list(Path("src/dolphin").rglob("*.c")) + list(Path("include/dolphin").rglob("*.h")):
        t = p.read_text(encoding="utf-8", errors="replace")
        for m in DEF.finditer(t):
            name, params = m.group(1), m.group(2)
            if name in ("if", "while", "for", "switch", "return", "sizeof") or "\n" in params and params.count("(") > 2:
                continue
            n, var = count_params(params.replace("\n", " "))
            if name not in ar or n > ar[name][0]:
                ar[name] = (n, var)
    return ar


def calls(text, name):
    for m in re.finditer(r"(?<![\w.>])" + re.escape(name) + r"\s*\(", text):
        i, depth = m.end(), 1
        while i < len(text) and depth:
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            i += 1
        inner = text[m.end():i - 1]
        line_start = text.rfind("\n", 0, m.start()) + 1
        line = text[line_start:text.find("\n", m.start())]
        if re.match(r"^\s*(extern|asm|static|void|s32|u32|f32|f64|BOOL|int|u8|u16|s8|s16)\b", line) and ";" in text[i:i + 3] and "=" not in line:
            continue  # a declaration, not a call
        if re.match(r"^[A-Za-z_][\w\s\*]*\b" + re.escape(name) + r"\s*\(", line):
            continue  # a definition line
        yield m.start(), inner


def main():
    ar = sdk_arities()
    names = re.compile(r"\b(" + "|".join(re.escape(n) for n in sorted(ar, key=len, reverse=True)) + r")\s*\(")
    hits = 0
    for root in ("src/game", "src/hsd", "src/musyx"):
        for p in sorted(Path(root).rglob("*.c")):
            t = p.read_text(encoding="utf-8", errors="replace")
            if not names.search(t):
                continue
            for name in set(names.findall(t)):
                want, var = ar[name]
                for pos, inner in calls(t, name):
                    got = 0 if not inner.strip() or inner.strip() == "void" else len(split_args(inner))
                    if got < want:
                        line = t.count("\n", 0, pos) + 1
                        if "--all" not in sys.argv and in_if0(t, pos):
                            continue
                        hits += 1
                        print(f"{p.as_posix()}:{line}: {name}: {got} of {want} arguments")
    print(f"{hits} short SDK calls", file=sys.stderr)


def in_if0(t, pos):
    """True if pos sits inside an `#if 0` block (simple nesting count)."""
    depth, zero = 0, []
    for m in re.finditer(r"^\s*#\s*(if|ifdef|ifndef|else|elif|endif)\b(.*)$", t[:pos], re.M):
        kw, rest = m.group(1), m.group(2).strip()
        if kw in ("if", "ifdef", "ifndef"):
            zero.append(kw == "if" and rest.startswith("0"))
        elif kw == "endif" and zero:
            zero.pop()
        elif kw in ("else", "elif") and zero:
            zero[-1] = False
    return any(zero)


if __name__ == "__main__":
    main()
