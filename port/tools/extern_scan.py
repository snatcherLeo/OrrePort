"""port/tools/extern_scan.py (B, 2026-10-17): Compare hand-written extern declarations in src/ with the prototypes in Aurora's and musyx-pc's headers.
Flags parameters where one side is a pointer / uintptr_t / 64-bit / float type and the other is a 32-bit integer
(dangerous past the 4th argument on Windows x64, and float vs int at any position). Read-only; prints a report.

Usage: python -I port/tools/extern_scan.py .
Compares against our own definitions in src/ first, then Aurora's / musyx-pc's headers; skips #ifndef TARGET_PC
blocks; only scans the files the PC build compiles (build-pc/firstlink/compiled.txt) when that list exists.
Limits: text matching, so K&R definitions and macros are not understood; argument-count differences need a look
by hand (often a GameCube-era declaration with fewer arguments). First results (2026-10-17): ARQPostRequest in gs_range_8017FA5C_exact_80180320.c (a crash on
re-entering a room) and sndSetAuxProcessingCallbacks in gs_range_801653CC.c (arguments 6-7 are pointers)."""
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
headers = list((root / "extern/aurora/include").rglob("*.h")) + list((root / "extern/musyx-pc/include").rglob("*.h"))

proto_re = re.compile(r"(?:^|[;{}\n])\s*(?:extern\s+)?(?:static\s+)?(?:inline\s+)?([A-Za-z_][\w\s\*]*?)\b([A-Za-z_]\w*)\s*\(([^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*;", re.S)


def pc_only(t):
    """Blank the lines the PC build does not see: #ifndef TARGET_PC / #if !defined(TARGET_PC) blocks and the #else of
    #ifdef TARGET_PC. Other conditions are kept (both branches), which is good enough for a review list."""
    out, stack = [], []  # stack entries: None (not about TARGET_PC) or True / False (this branch is seen by the PC)
    for line in t.split(chr(10)):
        s = line.strip()
        if s.startswith("#"):
            d = s[1:].strip()
            if d.startswith("ifndef") and "TARGET_PC" in d or re.match(r"if\s+!\s*defined\s*\(?\s*TARGET_PC", d):
                stack.append(False)
            elif d.startswith("ifdef") and "TARGET_PC" in d or re.match(r"if\s+defined\s*\(?\s*TARGET_PC", d):
                stack.append(True)
            elif d.startswith("if"):
                stack.append(None)
            elif d.startswith("else") and stack and stack[-1] is not None:
                stack[-1] = not stack[-1]
            elif d.startswith("endif") and stack:
                stack.pop()
            out.append("")
            continue
        out.append("" if any(x is False for x in stack) else line)
    return chr(10).join(out)


def strip_comments(t):
    t = re.sub(r"/\*.*?\*/", lambda m: chr(10) * m.group(0).count(chr(10)), t, flags=re.S)
    return re.sub(r"//[^\n]*", " ", t)


def params(text):
    depth, cur, out = 0, "", []
    for ch in text:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return [] if out == ["void"] else out


def kind(p):
    p = " ".join(p.split())
    if "(" in p or "*" in p or "ptr" in p.lower() or p.split()[0].endswith(("Callback", "Func")):
        return "ptr"
    if re.search(r"\b(uintptr_t|intptr_t|size_t|u64|s64|uint64_t|int64_t|long long|MUSY_HOST_USER)\b", p):
        return "wide"
    if re.search(r"\b(f32|f64|float|double)\b", p):
        return "float"
    if re.search(r"\b(u32|s32|u16|s16|u8|s8|int|unsigned|BOOL|char|short|uint32_t|int32_t|uint16_t|uint8_t|GX\w+|SND\w*ID|OSTime)\b", p):
        return "int"
    return "?"


real = {}
where = {}
for h in headers:
    t = strip_comments(h.read_text(encoding="utf-8", errors="replace"))
    for m in proto_re.finditer(t):
        if m.group(2) not in real:
            real[m.group(2)] = params(m.group(3))
            where[m.group(2)] = str(h.relative_to(root))

# Our own definitions in src/ win over the PC headers (a function src/musyx defines is called with that signature).
# Only definitions outside "#ifndef TARGET_PC" blocks would be exact; this keeps the first definition found.
def_re = re.compile(r"(?:^|\n)([A-Za-z_][\w \t\*]*?)\b([A-Za-z_]\w*)\s*\(([^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*\{")
own = {}
bodies = {}  # name -> the definition's body text (for "is this parameter ever read?")


def body_at(text, brace):
    """The text between the '{' at index brace and its matching '}'."""
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[brace + 1 : i]
    return text[brace + 1 :]


def param_name(p):
    """The parameter's name: the last identifier outside brackets (None for an unnamed parameter)."""
    p = re.sub(r"\[[^\]]*\]", "", p)
    m = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)", p)  # function pointer: (*name)(...)
    if m:
        return m.group(1)
    words = re.findall(r"[A-Za-z_]\w*", p)
    if len(words) < 2:
        return None
    return words[-1]
# Only the files the PC build compiles (the first link's list), when it exists: src/ holds several candidate copies
compiled_list = root / "build-pc/firstlink/compiled.txt"
compiled = set(compiled_list.read_text(encoding="utf-8").split()) if compiled_list.exists() else None
if compiled is not None:  # plus what port/CMakeLists.txt builds from src/ (B's MusyX library)
    cm = (root / "port/CMakeLists.txt").read_text(encoding="utf-8")
    compiled |= {m.group(1) for m in re.finditer(r"(?:\.\./|/)(src/[\w/]+\.c)\b", cm)}
for c in sorted((root / "src").rglob("*.c")):
    if compiled is not None and str(c.relative_to(root)).replace(chr(92), "/") not in compiled:
        continue
    t = pc_only(strip_comments(c.read_text(encoding="utf-8", errors="replace")))
    for m in def_re.finditer(t):
        name = m.group(2)
        if name in ("if", "for", "while", "switch", "return", "sizeof") or name in own:
            continue
        ps = params(m.group(3))
        if ps and all(" " in p or "*" in p or p == "..." for p in ps):  # skip K&R definitions (names only)
            own[name] = (ps, str(c.relative_to(root)))
            bodies[name] = body_at(t, m.end() - 1)
for name, (ps, path) in own.items():
    real[name] = ps
    where[name] = path

issues = 0
fewer = []  # (file:line, name, declared count, real count, calls in that file): a call leaves the callee's extra arguments as leftover registers
for c in sorted((root / "src").rglob("*.c")):
    if compiled is not None and str(c.relative_to(root)).replace(chr(92), "/") not in compiled:
        continue
    t = pc_only(strip_comments(c.read_text(encoding="utf-8", errors="replace")))
    for m in re.finditer(r"\bextern\s+([A-Za-z_][\w\s\*]*?)\b([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*;", t):
        name = m.group(2)
        if name not in real:
            continue
        mine, theirs = params(m.group(3)), real[name]
        if not mine or not theirs:
            continue
        bad = []
        for i, (a, b) in enumerate(zip(mine, theirs)):
            ka, kb = kind(a), kind(b)
            if "?" in (ka, kb) or ka == kb:
                continue
            if {ka, kb} <= {"ptr", "wide"}:
                continue
            if "float" in (ka, kb) or i >= 4:
                bad.append(f"arg {i + 1}: '{a}' vs '{b}'")
        if len(mine) != len(theirs):
            bad.append(f"{len(mine)} args vs {len(theirs)}")
        if bad:
            line = t[: m.start()].count("\n") + 1
            print(f"{c.relative_to(root)}:{line}: {name} (vs {where.get(name, '?')}): " + "; ".join(bad))
            issues += 1
            if len(mine) < len(theirs) and theirs != ["..."] and "..." not in theirs:
                calls = len(re.findall(r"(?<![\w*])" + re.escape(name) + r"\s*\(", t)) - 1  # minus the declaration
                if calls > 0:
                    # harmless when the definition never reads the parameters the declaration leaves out
                    body = bodies.get(name)
                    missing = [param_name(p) for p in theirs[len(mine):]]
                    unused = body is not None and all(
                        n is not None and not re.search(r"\b" + re.escape(n) + r"\b", body) for n in missing)
                    fewer.append((f"{c.relative_to(root)}:{line}", name, len(mine), len(theirs), calls, unused))
print(f"{issues} declarations to check")
# Fewer arguments declared than the definition takes, and called: the callee reads its missing arguments from
# whatever the registers / stack slots hold (the console's caller often left a real value there: trace it in the
# assembly, never default it to 0; the project rules / feedback on missing arguments).
real_ones = [f for f in fewer if not f[5]]
print(f"\n{len(real_ones)} called with fewer arguments than the definition takes, and it reads the missing ones:")
for where_, name, n_mine, n_theirs, calls, _ in sorted(real_ones, key=lambda x: (x[1], x[0])):
    print(f"  {where_}: {name}: declared {n_mine}, takes {n_theirs}; {calls} call(s) here")
harmless = [f for f in fewer if f[5]]
print(f"{len(harmless)} more where the definition never reads the missing parameters (harmless):")
for where_, name, n_mine, n_theirs, calls, _ in sorted(harmless, key=lambda x: (x[1], x[0])):
    print(f"  {where_}: {name}: declared {n_mine}, takes {n_theirs}")
