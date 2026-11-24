"""Compare hand-written function declarations in the game code with the functions' definitions (OrrePort, 2026-10-16).

Decompiled files often declare a callee themselves with stand-in types (`u32` for a pointer, `void f(void)` for a
function that takes arguments). On the GameCube that is harmless; on x86-64 it isn't:
  - a pointer parameter declared as a 32-bit integer arrives with garbage in its upper half when it is passed on the
    stack (the 5th and later arguments on Windows x64): sndUpdateListener's room pointer crashed MusyX this way;
  - a declaration with fewer parameters than the definition makes the callee read leftover registers (the zero-argument
    family; see also zeroargs.py and nativethunks.py's conflict list).
This lists every declaration that disagrees with the definition on parameter count, or on pointer vs integer vs float
for any parameter, worst first (stack-passed pointer mismatches, then count mismatches, then the rest).

Definitions: non-`asm` function bodies in src/ (game, hsd, musyx, dolphin). Declarations: every prototype line in
src/game, src/hsd, src/musyx and include/ that is not that definition, outside `#ifndef TARGET_PC` blocks (those are
GameCube-only; the PC build uses include/pc/prototypes.h for them). A heuristic text scan: verify each hit by hand.

Then the promotion check (2026-11-21, port-gaps 88; see the section further down): calls made through a declaration with
no parameter list (`T f();`) to a function whose definition takes an f32, with no prototype in scope, so the float
travels as a double and the callee reads garbage on x64. Its rows start with `PROMO`.

Usage (repo root): python -I tools/local/port/externcheck.py [--all] [--narrow]   (default: only what the PC compiles)
       python -I tools/local/port/externcheck.py --promo-only [--all] [--narrow]   (just the promotion check, seconds)
       python -I tools/local/port/externcheck.py --files A.c B.c [--protos P.h]    (promotion check on given files)
Output: build-pc/externcheck.txt (and a summary on stdout); --promo-only and --files print to stdout only.
Platform: a Python text scan, the same on every OS; what it finds matters on every 64-bit target (likely all).
"""
import bisect
import re
import sys
from pathlib import Path

DEF_DIRS = ["src/game", "src/hsd", "src/musyx", "src/dolphin"]
DECL_DIRS = ["src/game", "src/hsd", "src/musyx", "include"]
FUNC_RE = re.compile(r"^[ \t]*((?:extern\s+|static\s+|inline\s+)*[\w\s\*]*?)\b(\w+)\s*\(([^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*(?:/\*[^\n]*?\*/\s*)?([{;])", re.M)
SKIP_WORDS = {"if", "while", "for", "switch", "return", "sizeof", "else", "case", "do"}


def strip_comments(t):
    t = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), t, flags=re.S)
    return re.sub(r"//[^\n]*", "", t)


def drop_gamecube_only(t):
    """Blank `#ifndef TARGET_PC ... #endif` / `#else` regions (GameCube-only declarations), keeping line numbers."""
    out, stack = [], []
    for line in t.split("\n"):
        s = line.strip()
        if s.startswith("#if"):
            stack.append(s.startswith("#ifndef TARGET_PC") or s.startswith("#if !defined(TARGET_PC)"))
            out.append("")
            continue
        if s.startswith("#else") and stack:
            stack[-1] = not stack[-1] if stack[-1] is not None else None
            out.append("")
            continue
        if s.startswith("#endif") and stack:
            stack.pop()
            out.append("")
            continue
        out.append("" if any(stack) else line)
    return "\n".join(out)


def kinds(params):
    ps = [p.strip() for p in params.split(",")]
    if ps in ([""], ["void"]):
        return []
    out = []
    for p in ps:
        if p == "...":
            out.append("...")
        elif "*" in p or "[" in p or re.search(r"\(\s*\*", p):
            out.append("p")
        elif re.search(r"\b(f32|f64|float|double)\b", p):
            out.append("f")
        elif re.search(r"\b(u8|u16|u32|u64|s8|s16|s32|s64|int|unsigned|signed|char|short|long|BOOL|bool|size_t|uintptr_t)\b", p):
            out.append("i")
        else:
            out.append("?")  # a typedef or struct type the scan can't resolve (often a function pointer): never flagged
    return out


def scan(dirs, want_defs):
    found = {}
    for d in dirs:
        for f in sorted(Path(d).rglob("*")):
            if f.suffix not in (".c", ".h", ".cpp"):
                continue
            t = strip_comments(f.read_text(encoding="utf-8", errors="replace"))
            if not want_defs:
                t = drop_gamecube_only(t)
            for m in FUNC_RE.finditer(t):
                head, name, params, end = m.group(1), m.group(2), m.group(3), m.group(4)
                if name in SKIP_WORDS or not head.strip() or "asm" in head.split() or "typedef" in head:
                    continue
                if want_defs != (end == "{"):
                    continue
                if not want_defs and not params.strip():
                    continue  # `f()`: arguments unspecified in C (not `f(void)`); the zero-argument checkers cover those
                line = t.count("\n", 0, m.start()) + 1
                found.setdefault(name, []).append((f.as_posix(), line, kinds(params)))
    return found


def compiled_files():
    """The files the PC build compiles (build-pc/firstlink/compiled.txt) plus every src/ or include/ file they
    `#include "..."` by repo path, transitively; None when the list doesn't exist yet (no PC link has run)."""
    compiled = Path("build-pc/firstlink/compiled.txt")
    if not compiled.exists():
        return None
    keep = {l.strip() for l in compiled.read_text(encoding="utf-8").splitlines() if l.strip()}
    # 2026-11-06 (port-gaps 68): a compiled file may `#include "src/....c"` another .c file (the "parent" files of
    # the carved candidates); its declarations are compiled too. Before, they were dropped as "not compiled", which
    # hid itemUse2PokemonSimulation's stack-pointer mismatch in fight_trainer_ai_irekae_parent.c until P10A crashed.
    inc_re = re.compile(r'^\s*#\s*include\s+"((?:src|include)/[^"]+\.(?:c|cpp|h))"', re.M)
    todo = list(keep)
    while todo:
        f = todo.pop()
        p = Path(f)
        if not p.exists():
            continue
        for inc in inc_re.findall(p.read_text(encoding="utf-8", errors="replace")):
            if inc not in keep:
                keep.add(inc)
                todo.append(inc)
    return keep


# ---------------------------------------------------------------------------------------------------------------------
# Promotion check (2026-11-21, port-gaps 88): calls made through a declaration with NO parameter list.
#
# Why: `T f();` in C says nothing about f's parameters, so every argument of a call through it goes through the default
# argument promotions: a float is passed as a double, a char / short (u8, s8, u16, s16, bool) as an int. The PowerPC
# keeps float and double in the same register format, so the GameCube never showed it. On x86-64 a callee whose
# definition takes an `f32` reads a float from a register holding a double's bits: garbage (verified: Feather Dance's
# feathers, GSbezierCalculateVector / GSquatSlerp / fn_8018D680; it also changed the rolls after leaf moves). GCC's
# -Wdouble-promotion doesn't warn for these calls (it warns for variadic calls only; verified 2026-11-21), and the
# declaration comparison above skips `f()` on purpose, so this pass looks for the calls themselves.
#
# What it flags: a call, in a file the PC build compiles, to a function whose definition takes an `f32` / `float`
# parameter by value (FLOAT) or, with --narrow, a char / short-sized integer (NARROW), when no real prototype of it is
# in scope and an empty-parameter declaration is (in the file, in an enclosing block, or in a header the file
# includes). include/pc/prototypes.h counts as in scope everywhere: the PC build force-includes it, and GCC rejects an
# empty declaration of a function whose visible prototype takes a promotable type ("conflicting types"; verified
# 2026-11-21), so a compiling `f()` declaration always means no prototype was visible. Calls with no declaration at all
# are implicitdecl.py's job (GCC itself lists those). Also flagged (FLOAT?): a call through an empty declaration whose
# callee has no C definition the scan can read, when an argument is plainly a float (a `1.0f` literal or an
# `(f32)` / `(float)` cast).
#
# NARROW is off by default: the promoted int still carries the right low bits, and GCC's x64 callees read only those
# (likely harmless on Windows, Linux and macOS alike with GCC; clang assumes the caller extended narrow arguments to 32
# bits, which an int already is). Platform: the float half matters on every 64-bit target whose calling convention
# passes float and double differently in registers (Windows x64 and the System V ABI of Linux and macOS: likely all).
#
# A text scan, like the rest of this tool: `#ifndef TARGET_PC` / `#if 0` regions are dropped, other `#if` branches are
# all kept (so it errs toward flagging), macros are not expanded beyond the PC alias headers (`#define fn_X name`).
# ---------------------------------------------------------------------------------------------------------------------
PROMO_DEF_DIRS = DEF_DIRS + ["port/src"]  # port/src: functions the PC defines itself (SDK and runtime stand-ins)
INC_DIRS = ["include", "port/sdk_include", "build/GC6E01/include", ".", "extern/aurora/include", "port/src"]  # the PC compile's -I list
ALIAS_HEADERS = ["include/pc/game_pc_alias.h", "port/src/sdk_pc_alias.h"]
TOKEN_RE = re.compile(r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'", re.S)
FLOAT_ARG_RE = re.compile(r"(?:\b\d+\.\d*|\.\d+)(?:[eE][-+]?\d+)?[fF]\b|^\(\s*(?:f32|float)\s*\)")
CALL_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
# FUNC_RE with a head that crosses at most one line break: on the cleaned text (dead `#if` regions blanked) FUNC_RE's
# `[\w\s\*]*?` head ran across thousands of blank lines from every line start, 80 s for one data file
PROMO_FUNC_RE = re.compile(FUNC_RE.pattern.replace(r"[\w\s\*]*?", r"[\w \t\*]*?(?:\n[\w \t\*]*?)?", 1), re.M)
NOT_DECL_HEADS = SKIP_WORDS | {"goto", "throw", "new", "delete"}


def fine_kinds(params):
    """Per parameter: F = float by value, D = double, N = char / short-sized integer, p = pointer, i = other integer,
    . = variadic, ? = a type the scan can't resolve."""
    ps = [p.strip() for p in params.split(",")]
    if ps in ([""], ["void"]):
        return []
    out = []
    for p in ps:
        if p == "...":
            out.append(".")
        elif "*" in p or "[" in p or re.search(r"\(\s*\*", p):
            out.append("p")
        elif re.search(r"\b(f32|float)\b", p):
            out.append("F")
        elif re.search(r"\b(f64|double)\b", p):
            out.append("D")
        elif re.search(r"\b(u8|s8|u16|s16|char|short|bool|_Bool|GXBool)\b", p):
            out.append("N")
        elif re.search(r"\b(u32|u64|s32|s64|int|unsigned|signed|long|BOOL|size_t|uintptr_t)\b", p):
            out.append("i")
        else:
            out.append("?")
    return out


def clean_source(text):
    """Comments blanked (newlines kept), string and char literals emptied, dead preprocessor regions and every
    directive line blanked (line numbers kept). Returns (text, live quoted / angle includes)."""
    text = TOKEN_RE.sub(lambda m: ("\n" * m.group(0).count("\n")) if m.group(0)[0] == "/" else
                        ('""' if m.group(0)[0] == '"' else "' '"), text)
    lines = text.split("\n")
    out, stack, includes, i = [], [], [], 0
    while i < len(lines):
        line = lines[i]
        s = line.strip()
        if not s.startswith("#"):
            out.append("" if "dead" in stack else line)
            i += 1
            continue
        n = 1
        while line.endswith("\\") and i + n < len(lines):  # a directive continued over several lines
            line = line[:-1] + " " + lines[i + n]
            n += 1
        d = re.sub(r"^#\s*", "#", line.strip())
        if d.startswith("#if"):
            # dead on the PC's C compile: GameCube-only code, `#if 0`, and C++-only lines (a header's `extern "C" {`
            # under `#ifdef __cplusplus` would otherwise open a block around the whole header)
            if re.match(r"#ifndef\s+TARGET_PC\b|#if\s+!\s*defined\s*\(?\s*TARGET_PC\b|#if\s+0\b"
                        r"|#ifdef\s+__cplusplus\b|#if\s+defined\s*\(?\s*__cplusplus\s*\)?\s*$", d):
                stack.append("dead")
            elif re.match(r"#ifdef\s+TARGET_PC\b|#if\s+defined\s*\(?\s*TARGET_PC\s*\)?\s*$|#if\s+(TARGET_PC|1)\s*$", d):
                stack.append("pc")
            else:
                stack.append("live")  # an unknown condition: both branches kept
        elif d.startswith(("#else", "#elif")) and stack:
            stack[-1] = {"dead": "live" if d.startswith("#else") else "dead", "pc": "dead"}.get(stack[-1], "live")
        elif d.startswith("#endif") and stack:
            stack.pop()
        elif d.startswith("#include") and "dead" not in stack:
            m = re.match(r'#include\s*["<]([^">]+)[">]', d)
            if m:
                includes.append(m.group(1))
        out.extend([""] * n)
        i += n
    return "\n".join(out), includes


def load_aliases():
    alias = {}
    for h in ALIAS_HEADERS:
        p = Path(h)
        if p.exists():
            for m in re.finditer(r"^\s*#\s*define\s+(\w+)\s+(\w+)\s*$", p.read_text(encoding="utf-8", errors="replace"), re.M):
                alias[m.group(1)] = m.group(2)
    return alias


def scope_ends(text, positions):
    """For each offset in positions (sorted): where a declaration there stops being visible, the `}` closing its
    innermost enclosing block, or the end of file at file scope. One sweep over the braces. When the braces don't
    balance (an `#if` whose branches each open a block, say), every declaration counts as file scope, which errs
    toward flagging."""
    eof = len(text)
    braces = [(m.start(), m.group(0)) for m in re.finditer(r"[{}]", text)]
    close_of, stack = {}, []
    for pos, c in braces:
        if c == "{":
            stack.append(pos)
        elif not stack:
            return [eof] * len(positions)
        else:
            close_of[stack.pop()] = pos
    if stack:
        return [eof] * len(positions)
    out, stack, bi = [], [], 0
    for p in positions:
        while bi < len(braces) and braces[bi][0] < p:
            if braces[bi][1] == "{":
                stack.append(braces[bi][0])
            else:
                stack.pop()
            bi += 1
        out.append(close_of[stack[-1]] if stack else eof)
    return out


def split_args(text, open_paren):
    """The top-level arguments of the call whose `(` is at open_paren, or None if the parentheses don't close."""
    depth, args, start = 0, [], open_paren + 1
    for i in range(open_paren, min(len(text), open_paren + 4000)):
        c = text[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                args.append(text[start:i].strip())
                return [] if args == [""] else args
        elif c == "," and depth == 1:
            args.append(text[start:i].strip())
            start = i + 1
    return None


class PromoScan:
    def __init__(self, narrow=False, protos="include/pc/prototypes.h"):
        self.narrow = narrow
        self.alias = load_aliases()
        self.cache = {}
        # definitions: canonical name -> [(file, kinds, static)]
        self.defs = {}
        for d in PROMO_DEF_DIRS:
            for f in sorted(Path(d).rglob("*")):
                if f.suffix not in (".c", ".cpp"):
                    continue
                r = self.parsed(f.as_posix())
                for name, kinds_, static in (r[4] if r else []):
                    self.defs.setdefault(name, []).append((f.as_posix(), kinds_, static))
        # real prototypes visible in every PC unit: the force-included canonical list (--protos swaps in another copy,
        # to test the check against the list as it was before a fix)
        proto = self.parsed(protos)
        self.global_protos = {n for n, (kind, _pos, _line) in proto[2].items() if kind == "proto"} if proto else set()
        self.where_proto = {}  # canonical name -> first header line with a real prototype, for the report
        headers = sorted(p.as_posix() for p in Path("include").rglob("*.h") if p.as_posix() != "include/pc/prototypes.h")
        for f in [protos] + headers:
            r = self.parsed(f)
            if r:
                for n, (kind, _pos, line) in r[2].items():
                    if kind == "proto":
                        self.where_proto.setdefault(n, f"{f}:{line}")

    def canon(self, name):
        return self.alias.get(name, name)

    def parsed(self, path):
        """(clean text, includes, file-scope declarations {name: (proto | empty, pos, line)}, all declarations
        {name: [(kind, pos, line, scope end)]}, C definitions [(name, kinds, static)]) for a file, cached; None if it
        can't be read. Names are canonical (after the PC alias headers)."""
        if path in self.cache:
            return self.cache[path]
        p = Path(path)
        if not p.is_file():
            self.cache[path] = None
            return None
        t, incs = clean_source(p.read_text(encoding="utf-8", errors="replace"))
        found, defs_here = [], []
        for m in PROMO_FUNC_RE.finditer(t):
            head, name, params, end = m.groups()
            words = head.split()
            if name in SKIP_WORDS or not words or words[0] in NOT_DECL_HEADS or "typedef" in words:
                continue
            if end == "{" and "asm" not in words:
                defs_here.append((self.canon(name), fine_kinds(params), "static" in words))
            kind = "proto" if params.strip() else ("empty" if end == ";" else None)  # `T f() {` defines no prototype
            if kind is not None:
                found.append((m.start(2), name, kind, end))
        ends = scope_ends(t, [f[0] for f in found])
        nl = [m.start() for m in re.finditer("\n", t)]  # line numbers by bisection (the data files are megabytes)
        file_scope, decls = {}, {}  # decls: canonical name -> [(kind, pos, line, scope end)]
        for (pos, name, kind, end), end_pos in zip(found, ends):
            if end == "{":
                end_pos = len(t)  # a definition with parameters is a prototype for the rest of the file
            line = bisect.bisect_left(nl, pos) + 1
            canon = self.canon(name)
            decls.setdefault(canon, []).append((kind, pos, line, end_pos))
            if end_pos == len(t) and (canon not in file_scope or file_scope[canon][0] == "empty"):
                file_scope[canon] = (kind, pos, line)
        r = (t, incs, file_scope, decls, defs_here)
        self.cache[path] = r
        return r

    def resolve(self, inc, base):
        for root in [base] + [Path(d) for d in INC_DIRS]:
            cand = root / inc
            if cand.is_file():
                h = cand.as_posix()
                return h[2:] if h.startswith("./") else h
        return None  # a system header, or one the PC build doesn't have

    def header_decls(self, path):
        """{name: (kind, where)} of file-scope declarations in every header path includes, transitively (a real
        prototype anywhere wins over an empty declaration); memoised."""
        memo = self.__dict__.setdefault("_hd", {})
        if path in memo:
            return memo[path]
        memo[path] = {}  # guards include cycles
        out = {}
        r = self.parsed(path)
        if r:
            for inc in r[1]:
                h = self.resolve(inc, Path(path).parent)
                hr = self.parsed(h) if h else None
                if not hr:
                    continue
                own = {n: (kind, f"{h}:{line}") for n, (kind, _pos, line) in hr[2].items()}
                for src in (own, self.header_decls(h)):
                    for n, v in src.items():
                        if n not in out or out[n][0] == "empty":
                            out[n] = v
        memo[path] = out
        return out

    def definition(self, name, path):
        ds = self.defs.get(name, [])
        own = [d for d in ds if d[0] == path]
        if own:
            return own[0]
        ds = [d for d in ds if not d[2]]
        if ds and len({tuple(d[1]) for d in ds}) == 1:
            return ds[0]
        return None

    def scan_file(self, path):
        rows = []
        r = self.parsed(path)
        if not r:
            return rows
        t, decls = r[0], r[3]
        hdr = self.header_decls(path)
        empties = {n for n, ds in decls.items() if any(d[0] == "empty" for d in ds)}
        empties |= {n for n, v in hdr.items() if v[0] == "empty"}
        empties -= self.global_protos
        if not empties:
            return rows  # no empty-parameter declaration in reach: nothing to check
        decl_pos = {d[1] for ds in decls.values() for d in ds}
        for m in CALL_RE.finditer(t):
            name = self.canon(m.group(1))
            if name not in empties or m.start(1) in decl_pos:
                continue
            pos = m.start(1)
            visible = [(k, ln) for k, p, ln, e in decls.get(name, []) if p < pos <= e]
            if any(k == "proto" for k, _ in visible):
                continue
            through = f"{path}:{visible[-1][1]}" if visible else None
            if through is None:
                h = hdr.get(name)
                if not h or h[0] == "proto":
                    continue  # a header prototype, or no declaration at all (implicitdecl.py's family)
                through = h[1]
            args = split_args(t, m.end() - 1)
            if args is None:
                continue
            d = self.definition(name, path)
            line = t.count("\n", 0, pos) + 1
            if d is None:
                for i, a in enumerate(args):
                    if FLOAT_ARG_RE.search(a):
                        rows.append(("FLOAT?", name, f"{path}:{line}", i + 1, a, through, "no readable C definition"))
                continue
            kinds_ = d[1]
            for i, a in enumerate(args):
                if i >= len(kinds_) or kinds_[i] == ".":
                    break
                k = kinds_[i]
                if k == "F" or (k == "N" and self.narrow):
                    rows.append(("FLOAT" if k == "F" else "NARROW", name, f"{path}:{line}", i + 1, a, through,
                                 f"defined in {d[0]}"))
        return rows


def promotion_check(files, narrow=False, protos="include/pc/prototypes.h"):
    """Rows (label, function, file:line, argument number, argument text, empty declaration, definition, prototype)."""
    ps = PromoScan(narrow, protos)
    rows = []
    for f in sorted(files):
        if Path(f).suffix in (".c", ".cpp", ".h"):
            for row in ps.scan_file(f):
                rows.append(row + (ps.where_proto.get(row[1], "none"),))
    return rows


def write_promotion(rows, out):
    for label, name, at, argn, arg, through, defined, proto in rows:
        arg = arg if len(arg) <= 60 else arg[:57] + "..."
        out.write(f"PROMO {label:7s} {name}  {at}  (arg {argn}: {arg})  through empty declaration {through};"
                  f" {defined}; real prototype: {proto}\n")


def main():
    narrow = "--narrow" in sys.argv
    args = sys.argv[1:]
    protos = "include/pc/prototypes.h"
    if "--protos" in args:  # testing only: a stand-in for the force-included prototype list
        i = args.index("--protos")
        protos = args[i + 1].replace("\\", "/")
        del args[i:i + 2]
    # --files A B ...: run only the promotion check on these files (a scratch copy, for testing the check itself);
    # --promo-only: only the promotion check, on what the PC compiles (seconds instead of the full run's minutes)
    if "--files" in args or "--promo-only" in args:
        if "--files" in args:
            files = [a.replace("\\", "/") for a in args[args.index("--files") + 1:] if not a.startswith("--")]
        else:
            keep = None if "--all" in args else compiled_files()
            files = keep if keep is not None else {f.as_posix() for d in DECL_DIRS for f in Path(d).rglob("*")}
        rows = promotion_check(files, narrow, protos)
        write_promotion(rows, sys.stdout)
        print(f"promotion check: {len(rows)} call(s) through empty-parameter declarations")
        return
    defs = scan(DEF_DIRS, True)
    decls = scan(DECL_DIRS, False)
    # only what the PC build compiles (and headers, which compiled files include): unless --all
    keep = None if "--all" in sys.argv else compiled_files()
    if keep is not None:
        decls = {n: [d for d in ds if d[0] in keep or d[0].startswith("include/")] for n, ds in decls.items()}
        # a wrong declaration in a .c file only matters where that file calls the function (blanket declaration lists
        # of uncalled functions are harmless); headers stay in (any includer may call)
        texts = {}

        def called(f, name):
            if f.startswith("include/"):
                return True
            if f not in texts:
                texts[f] = strip_comments(Path(f).read_text(encoding="utf-8", errors="replace"))
            # uses minus declarations (2026-11-06: a file declaring a function twice, in both branches of an #ifdef,
            # counted as calling it; windowDrawSprite2's six rows)
            mentions = len(re.findall(r"\b" + re.escape(name) + r"\s*\(", texts[f]))
            # a declaration has a type before the name; a call statement (`f(x);`, `return f(x);`) does not
            declared = sum(1 for m in FUNC_RE.finditer(texts[f])
                           if m.group(2) == name and m.group(4) == ";" and m.group(1).strip()
                           and m.group(1).split()[0] not in SKIP_WORDS)
            return mentions > declared
        decls = {n: [d for d in ds if called(d[0], n)] for n, ds in decls.items()}
    rows = []
    for name, ds in decls.items():
        if name not in defs or len({tuple(k) for _, _, k in defs[name]}) != 1:
            continue  # no definition, or several disagreeing definitions (left for a person)
        dk = defs[name][0][2]
        if "..." in dk:
            continue
        for f, line, k in ds:
            if k == dk or "..." in k:
                continue
            score, why = 0, []
            if len(k) < len(dk):
                # fewer declared than defined: the callee reads leftover registers / stack (more declared is harmless:
                # the callee ignores the extra arguments)
                score = max(score, 2)
                why.append(f"{len(k)} params declared, {len(dk)} defined")
            for i, (a, b) in enumerate(zip(k, dk)):
                if a != b and "?" not in (a, b):
                    stack = i >= 4  # Windows x64: the 5th and later arguments go on the stack
                    if "p" in (a, b) and stack:
                        score = max(score, 3)
                    elif "p" in (a, b) or "f" in (a, b):
                        score = max(score, 1 if not stack else 2)
                    why.append(f"param {i + 1}: declared {a}, defined {b}{' (stack)' if stack else ''}")
            if score:
                rows.append((score, name, f, line, "; ".join(why), defs[name][0][0]))
    rows.sort(key=lambda r: (-r[0], r[1], r[2]))
    label = {3: "STACK POINTER", 2: "COUNT/STACK", 1: "TYPE"}
    Path("build-pc").mkdir(exist_ok=True)
    with open("build-pc/externcheck.txt", "w", encoding="utf-8") as out:
        for s, name, f, line, why, df in rows:
            out.write(f"{label[s]:13s} {name}  {f}:{line}  ({why})  defined in {df}\n")
        # the promotion check's rows follow (port-gaps 88): calls through `f()` declarations
        files = keep if keep is not None else {f.as_posix() for d in DECL_DIRS for f in Path(d).rglob("*")}
        promo = promotion_check(files, narrow)
        write_promotion(promo, out)
    counts = {label[s]: sum(1 for r in rows if r[0] == s) for s in (3, 2, 1)}
    print(f"declarations that disagree with the definition: {len(rows)} {counts} -> build-pc/externcheck.txt")
    pcounts = {k: sum(1 for r in promo if r[0] == k) for k in ("FLOAT", "FLOAT?") + (("NARROW",) if narrow else ())}
    print(f"calls through empty-parameter declarations that promote an argument: {len(promo)} {pcounts}"
          f"{'' if narrow else ' (narrow integers: --narrow)'} -> build-pc/externcheck.txt (PROMO rows)")


if __name__ == "__main__":
    main()
