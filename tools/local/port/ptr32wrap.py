"""Wrap every pointer-to-32-bit conversion in PC_PTR32() (OrrePort; snatcherLeo's option (a), condition 1).

The PC compiler lists every site: explicit casts of a pointer to a 32-bit integer ("cast from pointer to integer of
different size") and implicit conversions ("... makes integer from pointer without a cast" in initialisations,
assignments, arguments and returns). For each site this tool finds the expression in the source and rewrites
    (u32)expr        -> PC_PTR32(expr)          (also (s32), (int), (unsigned int), (u32 *const) ... 32-bit casts)
    (s32)expr        -> (s32)PC_PTR32(expr)
    <implicit> expr  -> PC_PTR32_IMPLICIT(expr)   (GameCube: the bare expression; local declarations differ there)
No include is added (an extra line would shift __LINE__ in HSD asserts, and an include can land inside an #if):
dolphin/types.h includes pc/ptr32.h for every file. On the GameCube PC_PTR32 is a plain (u32) cast, so the code must
stay byte-identical: verify with the full ninja, 26 SHA-1 and fullsnap 0 / 0. Casts to 8- or 16-bit types (taking the
low bits on purpose) and sites inside macro definitions are left alone. Overlapping sites are done in later passes.

Usage (repo root, MSYS2 mingw64 on PATH):
  python -I tools/local/port/ptr32wrap.py [--loads] [--only DIR] [--dry-run] [--passes N] [--exclude PREFIX ...]
--loads: the other direction, a 32-bit value turned back into a pointer: `(T*)u` -> `(T*)PC_PTR(u)` (the original cast
stays, so function-pointer types never pass through a macro argument), implicit `u` -> `PC_PTR(u)`.
"""
import collections
import concurrent.futures as cf
import json
import re
import subprocess
import sys
from pathlib import Path

only = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else None
DRY = "--dry-run" in sys.argv
LOADS = "--loads" in sys.argv  # 32-bit value -> pointer: (T*)u -> (T*)PC_PTR(u), implicit u -> PC_PTR(u)
PASSES = int(sys.argv[sys.argv.index("--passes") + 1]) if "--passes" in sys.argv else 4
EXCLUDE = [sys.argv[i + 1] for i, a in enumerate(sys.argv) if a == "--exclude"]
CR, LF = chr(13), chr(10)
CAST32 = re.compile(r"\(\s*(u32|s32|int|unsigned|unsigned int|signed int|long|unsigned long|BOOL)\s*\)")
NARROW = re.compile(r"\(\s*(u8|s8|u16|s16|char|unsigned char|signed char|short|unsigned short)\s*\)")

cmds = subprocess.run(["ninja", "-t", "commands", "all_source"], capture_output=True, text=True).stdout.splitlines()
units = []
for c in cmds:
    m = re.search(r" -c (\S+) -o \S+", c)
    if "mwcceppc.exe" not in c or not m:
        continue
    src = m.group(1).replace("\\", "/")
    if src.startswith(("src/game", "src/hsd", "src/musyx")) and (not only or src.startswith(only)):
        lang = "c++" if re.search(r"-lang[ =]c\+\+", c) or src.endswith((".cpp", ".cp")) else "c"
        units.append((src, re.findall(r"(?<=\s)-D(\S+)", c), lang))


def compile_unit(u):
    src, defs, lang = u
    cmd = ["gcc", "-fsyntax-only", "-fpermissive", "-fmax-errors=0", "-DTARGET_PC=1", "-include", "port/src/sdk_pc_alias.h", "-include", "include/pc/game_pc_alias.h", "-include", "include/pc/prototypes.h",
           "-Iinclude", "-Ibuild/GC6E01/include", "-I.", "-Iextern/aurora/include", "-x", lang,
           "-std=gnu11" if lang == "c" else "-std=gnu++17", "-Wint-conversion", "-Wpointer-to-int-cast", "-Wint-to-pointer-cast",
           "-fdiagnostics-format=json", "-fdiagnostics-column-unit=byte", "-fdiagnostics-column-origin=1"] + \
          [f"-D{d}" for d in defs] + [src]
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    try:
        return json.loads(r.stderr or "[]")
    except json.JSONDecodeError:
        return []


def sites():
    """{file: {(line, col, kind)}}: kind 'cast' (caret at the cast's '(') or 'implicit' (caret at the expression, or at
    '=' for an assignment)."""
    out = collections.defaultdict(set)
    with cf.ThreadPoolExecutor(16) as ex:
        for diags in ex.map(compile_unit, units):
            for d in diags:
                msg = d.get("message", "")
                if LOADS:
                    if "cast to pointer from integer of different size" in msg:
                        kind = "cast"
                    elif "makes pointer from integer" in msg or ("returning" in msg and "from a function with return type" in msg and "*" in msg.split("return type")[1]):
                        kind = "assign" if msg.startswith("assignment to") else "implicit"
                    else:
                        continue
                elif "cast from pointer to integer of different size" in msg:
                    kind = "cast"
                elif "makes integer from pointer" in msg or ("returning" in msg and "from a function with return type" in msg and "pointer" not in msg.split("return type")[1] and "*" in msg.split("return type")[0]):
                    kind = "assign" if msg.startswith("assignment to") else "implicit"
                else:
                    continue
                for loc in d.get("locations", [])[:1]:
                    car = loc["caret"]
                    f = car["file"].replace("\\", "/")
                    f = f[2:] if f.startswith("./") else f
                    if f.startswith(("src/game", "src/hsd", "src/musyx", "include/game", "include/hsd", "include/musyx")) and \
                            not any(f.startswith(x) for x in EXCLUDE):
                        out[f].add((car["line"], car["column"], kind))
    return out


OPEN, CLOSE = "([{", ")]}"


def skip_balanced(s, i):
    """s[i] is an opening bracket: return the index after its match (or -1)."""
    depth = 0
    while i < len(s):
        c = s[i]
        if c in "\"'":
            q = c
            i += 1
            while i < len(s) and s[i] != q:
                i += 2 if s[i] == "\\" else 1
        elif c in OPEN:
            depth += 1
        elif c in CLOSE:
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1


def is_fnptr_cast(s, i):
    """s[i] == '(' opens a function-pointer cast such as `(void(*)(void))` followed by its operand."""
    e = skip_balanced(s, i)
    if e < 0:
        return False
    inner = s[i + 1:e - 1]
    return "(*" in inner.replace(" ", "") and re.fullmatch(r"[\w\s\*\(\),]+", inner) is not None and \
        re.match(r"\s*[\w(&*]", s[e:]) is not None


def unary_end(s, i):
    """End index of the unary expression starting at s[i] (prefix ops, casts, primary, postfix ops)."""
    while i < len(s) and s[i] == " ":
        i += 1
    while True:  # prefix operators and nested casts
        if s.startswith(("++", "--"), i):
            i += 2
        elif i < len(s) and s[i] in "&*-!~+":
            i += 1
        elif s[i:i + 1] == "(" and (re.match(r"\(\s*(?:const\s+|volatile\s+|struct\s+|unsigned\s+|signed\s+)*\w+[\s\*]*\)", s[i:])
                                    and not re.match(r"\(\s*\w+\s*\)\s*[^\w\s(&*\-!~\"']", s[i:]) or is_fnptr_cast(s, i)):
            i = skip_balanced(s, i)  # a cast: skip it, the operand follows
        else:
            break
        while i < len(s) and s[i] == " ":
            i += 1
    if i >= len(s):
        return -1
    if s[i] in "([":
        i = skip_balanced(s, i)
    elif s[i] == '"':
        i = skip_balanced(s, i) if False else s.find('"', i + 1) + 1
    else:
        m = re.match(r"[\w\.]+", s[i:])
        if not m:
            return -1
        i += m.end()
    while i >= 0 and i < len(s):  # postfix: calls, indexing, members
        j = i
        while j < len(s) and s[j] == " ":
            j += 1
        if j < len(s) and s[j] in "([":
            i = skip_balanced(s, j)
        elif s.startswith(("->", ".", "++", "--"), j) and not s.startswith("...", j):
            k = j + (1 if s[j] == "." else 2)
            if s.startswith(("++", "--"), j):
                i = k
                continue
            m = re.match(r"\s*\w+", s[k:])
            if not m:
                return -1
            i = k + m.end()
        else:
            break
    return i


def expr_start(s, i):
    """Start of the full expression that contains s[i]: GCC puts the caret of a binary expression on its operator, so
    walk left to the unmatched ( [ , an argument comma, an assignment =, `return`, or the statement start."""
    depth = 0
    j = i - 1
    while j >= 0:
        c = s[j]
        if c in CLOSE:
            depth += 1
        elif c in OPEN:
            if depth == 0:
                return j + 1
            depth -= 1
        elif depth == 0:
            if c in ",;{}":
                return j + 1
            if c == "=" and s[j - 1:j] not in ("=", "!", "<", ">") and s[j + 1:j + 2] != "=":
                return j + 1  # assignment (also +=, |= ...: the operator char before is part of it)
            if s[j - 5:j + 1] == "return" and not (s[j - 6:j - 5].isalnum() or s[j - 6:j - 5] == "_"):
                return j + 1
        j -= 1
    return 0


def expr_end(s, i):
    """End of a full expression starting at s[i]: up to the next , ; ) ] } at depth 0."""
    while i < len(s):
        c = s[i]
        if c in OPEN:
            i = skip_balanced(s, i)
            if i < 0:
                return -1
            continue
        if c in "\"'":
            q = c
            i += 1
            while i < len(s) and s[i] != q:
                i += 2 if s[i] == "\\" else 1
            i += 1
            continue
        if c in ",;" or c in CLOSE:
            return i
        i += 1
    return -1


def rewrite(path, items):
    raw = Path(path).read_bytes().decode("utf-8", errors="surrogateescape")
    nl = CR + LF if (CR + LF) in raw else LF
    text = raw.replace(CR + LF, LF)
    starts = [0]
    for line in text.split(LF):
        starts.append(starts[-1] + len(line) + 1)
    edits = []  # (start, end, replacement)
    skipped = collections.Counter()
    for line, col, kind in sorted(items):
        if line - 1 >= len(starts) - 1:
            continue
        ls = text[starts[line - 1]:starts[line] - 1]
        if ls.lstrip().startswith("#"):
            skipped["macro or directive"] += 1
            continue
        # a line inside a multi-line #define (it, or the line before it, ends with a backslash): fix the macro itself
        prev = text[starts[line - 2]:starts[line - 1] - 1] if line >= 2 else ""
        if ls.rstrip().endswith("\\") or prev.rstrip().endswith("\\"):
            skipped["inside a multi-line #define"] += 1
            continue
        pos = starts[line - 1] + col - 1
        if kind == "cast" and LOADS:
            c = skip_balanced(text, pos) if text[pos:pos + 1] == "(" else -1
            e = unary_end(text, c) if c > 0 else -1
            if e < 0:
                skipped["cast or operand not parsed"] += 1
                continue
            operand = text[c:e].strip()
            if operand.startswith("PC_PTR("):
                continue
            edits.append((c, e, ("" if text[c:c + 1] != " " else " ") + f"PC_PTR({operand})"))
        elif kind == "cast":
            m = CAST32.match(text, pos)
            if not m:
                skipped["narrow or unrecognised cast" if NARROW.match(text, pos) else "unrecognised cast"] += 1
                continue
            e = unary_end(text, m.end())
            if e < 0:
                skipped["operand not parsed"] += 1
                continue
            operand = text[m.end():e].strip()
            ty = m.group(1)
            rep = f"PC_PTR32({operand})" if ty in ("u32", "unsigned", "unsigned int", "unsigned long") else f"({ty})PC_PTR32({operand})"
            edits.append((pos, e, rep))
        else:
            if kind == "assign":
                k = text.find("=", pos)
                if k < 0 or k > starts[line]:
                    skipped["assignment not found"] += 1
                    continue
                pos = k + 1
            if kind == "implicit":
                pos = expr_start(text, pos)
            while text[pos] in (" ", "\t", LF):
                pos += 1
            if text.startswith(("PC_PTR32(", "PC_PTR32_IMPLICIT(", "PC_PTR("), pos):
                continue
            e = expr_end(text, pos)
            if e < 0:
                skipped["expression not parsed"] += 1
                continue
            body = text[pos:e].rstrip()
            if not body or "\n#" in body:
                skipped["expression not parsed"] += 1
                continue
            if re.fullmatch(r"[A-Z][A-Z0-9_]*\s*\(.*\)", body, re.S) or re.search(r"\bcase\b|\bdefault\s*:|\bdo\b|[{;]|\\\n", body):
                skipped["inside a macro invocation"] += 1  # GCC reports the invocation: fix the macro definition instead
                continue
            edits.append((pos, pos + len(body), f"PC_PTR({body})" if LOADS else f"PC_PTR32_IMPLICIT({body})"))
    # keep only non-overlapping edits (innermost first); the rest are picked up in the next pass
    edits.sort(key=lambda x: (x[1] - x[0]))
    taken = []
    for s, e, r in edits:
        if all(e <= ts or s >= te for ts, te, _ in taken):
            taken.append((s, e, r))
        else:
            skipped["overlap (next pass)"] += 1
    if not taken:
        return 0, skipped
    for s, e, r in sorted(taken, reverse=True):
        text = text[:s] + r + text[e:]
    if not DRY:
        Path(path).write_bytes(text.replace(LF, nl).encode("utf-8", errors="surrogateescape"))
    return len(taken), skipped


total = 0
for p in range(PASSES):
    found = sites()
    n_sites = sum(len(v) for v in found.values())
    done = 0
    why = collections.Counter()
    for f, items in sorted(found.items()):
        k, sk = rewrite(f, items)
        done += k
        why.update(sk)
    total += done
    print(f"pass {p + 1}: {n_sites} sites in {len(found)} files; {done} wrapped; left: {dict(why)}")
    if DRY or done == 0:
        break
print(f"{total} sites wrapped in all{' (dry run, nothing written)' if DRY else ''}")
