"""structcopies.py: find "private struct copies" that are right on the GameCube but wrong on the PC (2026-10-17).

The bug family (docs/port/playbook.md): a file declares its own version of a record that other files also use, often
with padding arrays standing in for the fields it does not touch. With 4-byte pointers (GameCube) both versions have
the same size; on the PC (x64, 8-byte pointers) the other version grows, so an array allocated or indexed through the
private copy has the wrong stride. Two real cases on 2026-10-17: `SlotEntry` (gs_range_exact_8010C224.c) against
`FaceSlot` (gs_range_80109C88.c), which froze Willie's battle, and `WazaSequenceLocal` against the shared waza layout.

What it does (read-only, a heuristic):
  1. takes every C file the PC build compiles (build-pc/firstlink/compiled.txt, following `#include "src/....c"`
     wrappers), keeping only the TARGET_PC side of `#ifdef / #ifndef TARGET_PC` blocks;
  2. parses the struct definitions in those files and in include/, and computes each one's size for the GameCube
     (4-byte pointers) and the PC (8-byte pointers, x64 alignment); `PTR32_FIELD` counts 4 bytes on both;
  3. collects every `extern <Struct>* g;` / `extern <Struct> g[...]` declaration of a global and reports a global
     that two files view through different struct types whose sizes agree on the GameCube but differ on the PC
     ("likely"), or differ on both ("guess": maybe a deliberate partial view).

Usage (repo root): python -I tools/local/port/structcopies.py [--all] [--json OUT]
  --all  also lists globals whose views differ on both machines.
Reusable for XD (same code style); the file list comes from the Colosseum PC build.
"""
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

PRIM = {
    "u8": 1, "s8": 1, "char": 1, "bool": 1, "BOOL": 4, "u16": 2, "s16": 2, "short": 2, "u32": 4, "s32": 4, "int": 4,
    "unsigned": 4, "f32": 4, "float": 4, "long": 4, "u64": 8, "s64": 8, "f64": 8, "double": 8, "size_t": None,
    "uintptr_t": None, "intptr_t": None, "Vec": 12, "Mtx": 48, "GXColor": 4,
}


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def pc_side(text):
    """Keep the TARGET_PC side of simple #ifdef / #ifndef / #if defined(TARGET_PC) blocks; other conditions stay."""
    out, stack = [], []  # stack of (this_branch_active, condition_is_target_pc)
    for line in text.split("\n"):
        s = line.strip()
        m = re.match(r"#\s*(ifdef|ifndef|if)\b(.*)", s)
        if m:
            kind, cond = m.group(1), m.group(2)
            tp = "TARGET_PC" in cond
            if tp:
                active = (kind == "ifdef") or (kind == "if" and "!" not in cond)
            else:
                active = True
            stack.append([active, tp])
            continue
        if re.match(r"#\s*else\b", s) and stack:
            if stack[-1][1]:
                stack[-1][0] = not stack[-1][0]
            continue
        if re.match(r"#\s*elif\b", s) and stack:
            continue
        if re.match(r"#\s*endif\b", s) and stack:
            stack.pop()
            continue
        if all(a for a, _ in stack):
            out.append(line)
    return "\n".join(out)


def tu_files(src):
    """A compiled file plus the src/ C files it #includes (the exact / candidate wrapper pattern)."""
    seen, todo = [], [src]
    while todo:
        f = todo.pop()
        if f in seen or not (ROOT / f).exists():
            continue
        seen.append(f)
        for inc in re.findall(r'#\s*include\s+"((?:src|\./src)/[^"]+\.c(?:pp)?)"', (ROOT / f).read_text("utf-8", "replace")):
            todo.append(inc.lstrip("./"))
    return seen


def match_brace(text, i):
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return j
    return -1


def parse_structs(text):
    """{name: body} for `typedef struct [tag] { ... } Name;` and `struct Tag { ... };`."""
    found = {}
    for m in re.finditer(r"\b(typedef\s+)?(struct|union)\s*(\w*)\s*\{", text):
        start = m.end() - 1
        end = match_brace(text, start)
        if end < 0:
            continue
        body = text[start + 1:end]
        after = text[end + 1:end + 80]
        names = []
        if m.group(1):
            n = re.match(r"\s*(\w+)\s*;", after)
            if n:
                names.append(n.group(1))
        if m.group(3):
            names.append(m.group(3))
        for n in names:
            found.setdefault(n, (m.group(2), body))
    return found


class Layout:
    def __init__(self, structs, aliases):
        self.structs, self.aliases, self.cache = structs, aliases, {}

    def type_size(self, t, ptr):
        """(size, align) of a type name; None if unknown."""
        t = t.replace("const", "").replace("volatile", "").replace("struct", "").replace("union", "").strip()
        t = re.sub(r"\bunsigned\s+(char|short|int|long)\b", r"\1", t).strip()
        if t in PRIM:
            s = PRIM[t]
            if s is None:
                s = ptr
            return s, min(s, 8)
        if t in self.aliases:
            return self.type_size(self.aliases[t], ptr)
        if t in self.structs:
            r = self.size(t, ptr)
            return r
        return None

    def size(self, name, ptr):
        key = (name, ptr)
        if key in self.cache:
            return self.cache[key]
        self.cache[key] = None  # recursion guard
        kind, body = self.structs[name]
        r = self.body_size(kind, body, ptr)
        self.cache[key] = r
        return r

    def body_size(self, kind, body, ptr):
        off, align, unknown = 0, 1, False
        sizes = []
        i = 0
        # split into top-level member declarations, keeping nested braces together
        decls, cur, depth = [], "", 0
        for ch in body:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
            if ch == ";" and depth == 0:
                decls.append(cur.strip())
                cur = ""
            else:
                cur += ch
        for d in decls:
            if not d:
                continue
            nested = re.match(r"(struct|union)\s*\w*\s*\{(.*)\}\s*(.*)$", d, flags=re.S)
            if nested:
                r = self.body_size(nested.group(1), nested.group(2), ptr)
                if r is None:
                    return None
                items = [(r[0], r[1], nested.group(3) or "x")]
            elif d.startswith("PTR32_FIELD"):
                n = re.match(r"PTR32_FIELD\s*\(.*?\)\s*(\w+)\s*(\[[^\]]*\])?", d)
                cnt = self.array_count(n.group(2)) if n and n.group(2) else 1
                if cnt is None:
                    return None
                items = [(4 * cnt, 4, "")]
            elif "(*" in d:
                items = [(ptr, ptr, "")]
            else:
                if ":" in d and re.search(r":\s*\d+", d):
                    unknown = True  # bit-fields: approximate by the base type once
                    d = re.sub(r":\s*\d+", "", d)
                m = re.match(r"([\w\s]+?)\s*((?:\*\s*)*)\s*(\w+)\s*((?:\[[^\]]*\])*)\s*((?:,.*)?)$", d, flags=re.S)
                if not m:
                    return None
                base, stars, rest = m.group(1).strip(), m.group(2), m.group(5)
                declarators = [(stars, m.group(4))]
                for extra in re.findall(r",\s*((?:\*\s*)*)\s*\w+\s*((?:\[[^\]]*\])*)", rest):
                    declarators.append(extra)
                items = []
                for st, arr in declarators:
                    if "*" in st:
                        s, a = ptr, ptr
                    else:
                        r = self.type_size(base, ptr)
                        if r is None:
                            return None
                        s, a = r
                    cnt = 1
                    for dim in re.findall(r"\[([^\]]*)\]", arr):
                        c = self.array_count("[" + dim + "]")
                        if c is None:
                            return None
                        cnt *= c
                    items.append((s * cnt, a, ""))
            for s, a, _ in items:
                align = max(align, a)
                if kind == "union":
                    sizes.append(s)
                else:
                    off = (off + a - 1) // a * a + s
        if kind == "union":
            off = max(sizes) if sizes else 0
        off = (off + align - 1) // align * align
        return off, align

    @staticmethod
    def array_count(dim):
        expr = dim.strip("[]").strip()
        if not expr:
            return 0
        if not re.fullmatch(r"[0-9xXa-fA-F\s+\-*/()]+", expr):
            return None
        try:
            return int(eval(expr, {"__builtins__": {}}))  # numbers and arithmetic only (checked above)
        except Exception:
            return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--json")
    args = ap.parse_args()

    compiled = [l.strip() for l in (ROOT / "build-pc/firstlink/compiled.txt").read_text("utf-8").splitlines()
                if l.strip().startswith("src/")]

    header_structs, aliases = {}, {}
    for h in list((ROOT / "include").rglob("*.h")):
        t = pc_side(strip_comments(h.read_text("utf-8", "replace")))
        for n, v in parse_structs(t).items():
            header_structs.setdefault(n, (v, str(h.relative_to(ROOT)).replace("\\", "/")))
        for a, b in re.findall(r"typedef\s+([\w\s]+?)\s+(\w+)\s*;", t):
            if not a.strip().startswith(("struct", "union", "enum")):
                aliases.setdefault(b, a.strip())

    views = {}  # global -> list of (file, struct, gc, pc, defined_in)
    for src in compiled:
        files = tu_files(src)
        text = "\n".join(pc_side(strip_comments((ROOT / f).read_text("utf-8", "replace"))) for f in files)
        local = {n: v for n, v in parse_structs(text).items()}
        structs = {n: v for n, (v, _) in header_structs.items()}
        structs.update(local)
        lay = Layout(structs, aliases)
        for typ, glob in re.findall(r"\bextern\s+(?:struct\s+)?(\w+)\s*\*\s*(\w+)\s*;", text) + \
                [(t, g) for t, g in re.findall(r"\bextern\s+(?:struct\s+)?(\w+)\s+(\w+)\s*\[", text)]:
            if typ not in structs:
                continue
            gc, pc = lay.size(typ, 4), lay.size(typ, 8)
            if gc is None or pc is None:
                continue
            where = "local" if typ in local else header_structs[typ][1]
            views.setdefault(glob, []).append((files[-1] if len(files) == 1 else files[0], typ, gc[0], pc[0], where))

    hits = []
    for glob, vs in views.items():
        kinds = {(v[1], v[2], v[3]) for v in vs}
        if len(kinds) < 2:
            continue
        gcs = {k[1] for k in kinds}
        pcs = {k[2] for k in kinds}
        if len(pcs) < 2:
            continue
        label = "likely" if len(gcs) == 1 else "guess"
        if label == "guess" and not args.all:
            continue
        seen, rows = set(), []
        for f, t, g, p, w in vs:
            if (f, t) in seen:
                continue
            seen.add((f, t))
            rows.append({"file": f, "struct": t, "gc": g, "pc": p, "defined": w})
        hits.append({"global": glob, "label": label, "views": rows})

    hits.sort(key=lambda h: (h["label"] != "likely", h["global"]))
    for h in hits:
        print(f"{h['global']}  [{h['label']}]")
        for r in h["views"]:
            print(f"    {r['struct']:<28} GC {r['gc']:>5}  PC {r['pc']:>5}  {r['file']}  ({r['defined']})")
    print(f"\n{sum(h['label'] == 'likely' for h in hits)} likely, {sum(h['label'] == 'guess' for h in hits)} guess "
          f"(globals viewed through struct types whose PC sizes differ)", file=sys.stderr)
    if args.json:
        Path(args.json).write_text(json.dumps(hits, indent=1), encoding="utf-8")


if __name__ == "__main__":
    main()
