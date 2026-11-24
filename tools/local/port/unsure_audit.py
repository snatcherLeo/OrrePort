"""The PC build's "unsure" data symbols (pcdata.py left them raw big-endian bytes) against the code that reads them
(2026-10-25, after the battle item list lbl_80375D70 turned out to be read natively).

    python -I tools/local/port/unsure_audit.py refs <decodedline.txt> > refs.tsv
    python -I tools/local/port/unsure_audit.py flow <symbol> [lines after]

decodedline.txt: `objdump --dwarf=decodedline build-pc/firstlink/orrecolo.exe` (the source lines the PC exe really
compiles; files by base name). Run from the repo root.

refs: every reference to an unsure symbol whose raw bytes aren't all zero (a table of zeros reads the same in either
byte order) on a compiled line, one TSV row: symbol, size, class, file:line, the line. Classes: index-byte (sym[i]
with a byte type in that file), index-wide (sym[i] with a wider declared type), cast (a cast to a non-byte pointer),
arg:<callee> (passed to a call: the callee's reads decide), other.

flow: variables assigned from the symbol (and variables assigned from those) that are then read natively within the
next lines: *(T*)(var ...) with T 16 / 32 bits, and var[i] / *var for a wide pointer variable, not through DISC_*.
Each hit is a candidate (check it is compiled: the refs mode's line set) and needs a human read: a variable reused
for something else in a later function shows up too.

Not covered: readers reached only through another table's pointer or through more than one call level, and typed
struct views (check them for DISC_STRUCT by hand).
"""
import os
import re
import sys

BYTE = {"u8", "s8", "char", "unsigned char", "signed char", "const u8", "const char", "const s8"}


def unsure_nonzero():
    """unsure symbols whose generated raw bytes aren't all zero: {name: size}"""
    out = {}
    for ln in open("build-pc/generated/pc_data_main.c", encoding="utf-8", errors="replace"):
        if "unsure: raw big-endian" not in ln:
            continue
        m = re.match(r"u8 (lbl_[0-9A-F]{8})\[(\d+)\]", ln)
        if m and any(int(x, 16) for x in re.findall(r"0x[0-9A-F]{2}", ln[ln.find("{"):ln.rfind("}")])):
            out[m.group(1)] = int(m.group(2))
    return out


def compiled_lines(path):
    comp = set()
    for ln in open(path, encoding="utf-8", errors="replace"):
        m = re.match(r"(\S+\.[ch])\s+(\d+)\s+0x[0-9a-f]+", ln)
        if m:
            comp.add((m.group(1), int(m.group(2))))
    return comp


def c_files():
    for dp, _, fns in os.walk("src"):
        for fn in fns:
            if fn.endswith(".c"):
                yield os.path.join(dp, fn), fn


def refs(dl_path):
    syms = unsure_nonzero()
    compiled = compiled_lines(dl_path)
    pat = re.compile(r"\b(lbl_[0-9A-F]{8})\b")
    decl_re = re.compile(r"extern\s+(const\s+)?([\w ]+?)\s*\**\s*\b(lbl_[0-9A-F]{8})\b")
    for path, fn in c_files():
        text = open(path, encoding="utf-8", errors="replace").read().split("\n")
        decls = {}
        for t in text:
            for m in decl_re.finditer(t):
                if m.group(3) in syms:
                    decls[m.group(3)] = ((m.group(1) or "") + m.group(2)).strip()
        for i, t in enumerate(text, 1):
            if "lbl_" not in t or (fn, i) not in compiled:
                continue
            for m in pat.finditer(t):
                sym = m.group(1)
                if sym not in syms:
                    continue
                before, after = t[:m.start()], t[m.end():]
                cls = "other"
                cm = re.search(r"\(\s*(const\s+)?([\w ]+?)\s*(\*+)\s*\)\s*\(?\s*&?\s*$", before)
                if cm and cm.group(2).strip() not in BYTE and cm.group(2).strip() != "void":
                    cls = "cast:" + cm.group(2).strip() + cm.group(3)
                elif re.match(r"\s*\[", after):
                    ty = decls.get(sym, "?")
                    cls = ("index-byte:" if ty in BYTE else "index-wide:") + ty
                else:
                    depth, j, callee = 0, len(before) - 1, None
                    while j >= 0:
                        c = before[j]
                        if c == ")":
                            depth += 1
                        elif c == "(":
                            if depth == 0:
                                k = re.search(r"(\w+)\s*$", before[:j])
                                callee = k.group(1) if k else None
                                break
                            depth -= 1
                        j -= 1
                    if callee and callee not in ("if", "while", "for", "switch", "return", "sizeof"):
                        cls = "arg:" + callee
                print("\t".join([sym, f"0x{syms[sym]:X}", cls, f"{path.replace(chr(92), '/')}:{i}", t.strip()[:160]]))


def flow(sym, span):
    assign = re.compile(r"\b(\w+)\s*=\s*[^;=]*\b" + sym + r"\b")
    for path, _ in c_files():
        lines = open(path, encoding="utf-8", errors="replace").read().split("\n")
        for i, t in enumerate(lines):
            m = assign.search(t)
            if not m or "DISC_" in t[:m.start()]:
                continue
            var = m.group(1)
            if re.search(r"\(\s*(s16|u16|s32|u32|f32)\s*\*\s*\)", t[m.start():]):
                for j in range(i, min(len(lines), i + span)):
                    u = lines[j] if j > i else t[m.end():]
                    for rm in re.finditer(r"(?<![\w.>])(\*\s*" + var + r"\b(?!\s*=)|\b" + var + r"\s*\[)", u):
                        if "DISC_" not in u[max(0, rm.start() - 14):rm.start()]:
                            print(f"{path}:{j + 1}: [{var} wide from line {i + 1}] {u.strip()[:150]}")
            vars_ = {var}
            for j in range(i + 1, min(len(lines), i + span)):
                u = lines[j]
                for v in list(vars_):
                    am = re.search(r"\b(\w+)\s*=\s*[^;=]*\b" + v + r"\b", u)
                    if am and am.group(1) not in vars_ and "DISC_" not in u:
                        vars_.add(am.group(1))
                for v in vars_:
                    for rm in re.finditer(r"\*\s*\(\s*(const\s+)?(s16|u16|s32|u32|f32|int|short)\s*\*\s*\)\s*\(?\s*\(?"
                                          r"\s*(u8\s*\*\s*\))?\s*" + v + r"\b", u):
                        if "DISC_" not in u[:rm.start()][-12:]:
                            print(f"{path}:{j + 1}: [{v} from line {i + 1}] {u.strip()[:150]}")


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "refs":
        refs(sys.argv[2])
    elif len(sys.argv) >= 3 and sys.argv[1] == "flow":
        flow(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 40)
    else:
        sys.exit(__doc__)
