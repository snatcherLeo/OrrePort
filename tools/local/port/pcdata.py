"""Generate C definitions for the game data the PC build needs but no C file defines (OrrePort, Main queue #1).

The decompiled C references thousands of data symbols (`lbl_...` variables, tables, float constants). Many are
defined in C; the rest exist only as bytes in the original program. This tool reads those bytes from the build's
disc-derived objects (`build/GC6E01/obj/**.o`, the ELF files dtk split from the original main.dol: symbols, sizes,
`R_PPC_ADDR32` relocations) and writes C definitions the PC build compiles and links.

snatcherLeo's conditions (2026-10-15, the project notes section 6):
1. **Output is game data:** written only to the untracked `build-pc/generated/`, every file starting with the marker
   line; the git hooks in tools/local/hooks refuse to commit it. Committed: this tool and `pcdata_types.txt`
   (hand-written type info).
2. **Byte order per field, from type info:** a symbol is converted only when its element type is known and the
   sources agree (the C declarations, dtk's access hints in symbols.txt, the relocations). Anything else is
   **unsure**: emitted as the untouched original bytes (big-endian, bit-exact) and listed in `unsure_data.txt`, which the
   Dolphin comparisons check first.
3. **Pointers:** a word with a relocation is a pointer. In a symbol declared with a pointer type it becomes a real
   pointer (`&target`); in a GameCube-layout slot (u32 arrays, byte blobs) it becomes a startup relocation
   (`pc_data_relocate()` writes the target's PC address, which fits in 32 bits in OrrePort's fixed memory layout).

Outputs (build-pc/generated/): pc_data_main.c, unsure_data.txt, pcdata_report.txt.
Usage (repo root):
  python -I tools/local/port/pcdata.py [--need NAMES.txt] [--defined NAMES.txt] [--link-defined NAMES.txt]
                                       [--out build-pc/generated]
      --defined: symbols another part of the PC build defines itself with real types (e.g. MusyX state in
      extern/musyx-pc); they are never generated, and references to them stay extern.
      --link-defined: every data name the compiled PC objects define (nm over the game objects that compiled, one
      per line). When given, it replaces the source scan's "defined in C" set: a definition inside a block the PC
      build turns off (gs_vm.c's GS_VM_WHOLE_UNIT), in a file configure.py doesn't build (camera.c) or in a unit
      that failed to compile is not a definition the link has, so the name is generated from the disc bytes, which
      are the bytes the gated C was matched against.
      --need: generate exactly these symbols (e.g. the linker's undefined data list), plus the main-program data the
      generated room modules reference (pcreldata.py); default: data symbols the C references but no C file defines
      (a source scan).
"""
import collections
import re
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from datanames import DATA_NAME, sort_key  # noqa: E402  (`lbl_<addr>` or a renamed data label; docs/data_renames.csv)

OBJ_ROOT = Path("build/GC6E01/obj")
SYMBOLS = Path("config/GC6E01/symbols.txt")
TYPES_FILE = Path("tools/local/port/pcdata_types.txt")
MARKER = "ORREPORT-" + "GENERATED-FROM-DISC"
R_PPC_ADDR32 = 1

SCALARS = {  # C type -> (size, struct format, kind)
    "u8": (1, "B", "int"), "s8": (1, "b", "int"), "char": (1, "b", "int"), "unsigned char": (1, "B", "int"),
    "u16": (2, "H", "int"), "s16": (2, "h", "int"), "short": (2, "h", "int"), "unsigned short": (2, "H", "int"),
    "u32": (4, "I", "int"), "s32": (4, "i", "int"), "int": (4, "i", "int"), "unsigned int": (4, "I", "int"),
    "BOOL": (4, "i", "int"), "unsigned": (4, "I", "int"), "long": (4, "i", "int"), "unsigned long": (4, "I", "int"),
    "u64": (8, "Q", "int"), "s64": (8, "q", "int"),
    "f32": (4, "f", "float"), "float": (4, "f", "float"), "f64": (8, "d", "float"), "double": (8, "d", "float"),
}
BYTE_TYPES = {"u8", "s8", "char", "unsigned char", "signed char"}  # "just bytes" declarations (raw blob externs)
HINT_SIZE = {"byte": 1, "2byte": 2, "4byte": 4, "8byte": 8, "float": 4, "double": 8}


# ---------------------------------------------------------------- ELF (32-bit big-endian, PowerPC)
def read_elf(path):
    """{symbol: (section name, bytes, [(offset in symbol, target name, addend, type)])} for defined object symbols."""
    b = path.read_bytes()
    if b[:4] != b"\x7fELF":
        return {}
    shoff, = struct.unpack(">I", b[0x20:0x24])
    shentsize, shnum, shstrndx = struct.unpack(">HHH", b[0x2E:0x34])
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsize
        name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack(">IIIIIIIIII", b[o:o + 40])
        secs.append(dict(name=name, type=typ, off=off, size=size, link=link, info=info, entsize=entsize))
    shstr = secs[shstrndx]

    def sname(n):
        s = b[shstr["off"] + n:]
        return s[:s.index(b"\0")].decode("latin-1")

    for s in secs:
        s["sname"] = sname(s["name"])
    symtab = next((s for s in secs if s["type"] == 2), None)
    if symtab is None:
        return {}
    strtab = secs[symtab["link"]]

    def strn(n):
        s = b[strtab["off"] + n:]
        return s[:s.index(b"\0")].decode("latin-1")

    syms = []
    for i in range(symtab["size"] // 16):
        o = symtab["off"] + 16 * i
        name, value, size, info, other, shndx = struct.unpack(">IIIBBH", b[o:o + 16])
        syms.append((strn(name), value, size, info & 0xF, shndx))
    out = {}
    per_sec = collections.defaultdict(list)
    for name, value, size, typ, shndx in syms:
        if typ == 1 and 0 < shndx < len(secs) and name:  # STT_OBJECT
            per_sec[shndx].append((name, value, size))
    relocs = collections.defaultdict(list)  # target section index -> [(offset, symbol name, addend, type)]
    for s in secs:
        if s["type"] == 4:  # SHT_RELA
            for i in range(s["size"] // 12):
                o = s["off"] + 12 * i
                r_off, r_info, r_add = struct.unpack(">IIi", b[o:o + 12])
                relocs[s["info"]].append((r_off, syms[r_info >> 8][0], r_add, r_info & 0xFF))
    for shndx, lst in per_sec.items():
        sec = secs[shndx]
        nobits = sec["type"] == 8
        for name, value, size in lst:
            data = bytes(size) if nobits else b[sec["off"] + value: sec["off"] + value + size]
            rel = [(ro - value, t, a, ty) for ro, t, a, ty in relocs.get(shndx, []) if value <= ro < value + size]
            out[name] = (sec["sname"], data, rel, nobits)
    return out


# ---------------------------------------------------------------- symbols.txt and the C sources
def read_symbols():
    """name -> (section, address, size, kind ('object'/'function'), dtk data hint or None)."""
    out = {}
    pat = re.compile(r"^(\S+) = (\.\w+):0x([0-9A-Fa-f]+); // type:(\w+)(?: size:0x([0-9A-Fa-f]+))?(.*)$")
    for line in SYMBOLS.read_text(encoding="utf-8").splitlines():
        m = pat.match(line)
        if m:
            hint = re.search(r"\bdata:(\w+)", m.group(6) or "")
            out[m.group(1)] = (m.group(2), int(m.group(3), 16), int(m.group(5) or "0", 16), m.group(4),
                               hint.group(1) if hint else None)
    # pcblocks.txt (2026-10-18, upstream merge 4e36aa0de): zero-filled work areas that symbols.txt now splits into
    # small labels, while the code (room modules: base pointer + offsets) uses each as one block. The PC keeps the
    # block whole at its earlier size and the inner labels become aliases at their GameCube offsets (firstlink reads
    # pc_block_aliases.txt). Written by merge_datacheck.py --write-blocks.
    BLOCK_ALIASES.clear()
    KEPT_BLOCKS.clear()
    if BLOCKS_FILE.exists():
        for line in BLOCKS_FILE.read_text(encoding="utf-8").splitlines():
            w = line.split("#")[0].split()
            if len(w) != 2 or w[0] not in out:
                continue
            sec, addr, _, kind, hint = out[w[0]]
            size = int(w[1], 16)
            out[w[0]] = (sec, addr, size, kind, hint)
            KEPT_BLOCKS.add(w[0])
            for n, (s2, a2, z2, k2, h2) in list(out.items()):
                if s2 == sec and addr < a2 < addr + size:
                    del out[n]
                    BLOCK_ALIASES.append((n, w[0], a2 - addr))
    return out


BLOCKS_FILE = Path("tools/local/port/pcblocks.txt")
BLOCK_ALIASES = []
KEPT_BLOCKS = set()


FIXED_FILE = Path("tools/local/port/pcfixed.txt")
DOL = Path("orig/GC6E01/sys/main.dol")


def read_fixed():
    """pcfixed.txt (option 1, port-gaps 22): ([(lo, hi)] raw ranges, {name: element type} variables)"""
    raws, fvars = [], {}
    if not FIXED_FILE.exists():
        return raws, fvars
    for line in FIXED_FILE.read_text(encoding="utf-8").splitlines():
        w = line.split("#", 1)[0].split()
        if not w:
            continue
        if w[0] == "raw" and len(w) == 3:
            raws.append((int(w[1], 16), int(w[2], 16)))
        elif w[0] == "var" and len(w) == 3 and w[2] in ("u8", "u16", "u32"):
            fvars[w[1]] = w[2]
        else:
            raise SystemExit(f"pcdata: {FIXED_FILE}: can't read the line {line!r}")
    return raws, fvars


def dol_bytes(addr, size):
    """main.dol's bytes at a GameCube address (None when they are not in the file: .bss / .sbss)"""
    d = dol_bytes.cache = getattr(dol_bytes, "cache", None) or DOL.read_bytes()
    offs, addrs, sizes = (struct.unpack(">18I", d[i:i + 72]) for i in (0, 72, 144))
    for o, s, z in zip(offs, addrs, sizes):
        if z and s <= addr and addr + size <= s + z:
            return d[o + addr - s:o + addr - s + size]
    return None


def scan_sources():
    """(declared types per data name, names defined in C, names referenced in C)."""
    decls = collections.defaultdict(collections.Counter)
    defined, referenced = set(), set()
    pat_ext = re.compile(r"\bextern\s+((?:const\s+|volatile\s+|unsigned\s+|signed\s+|struct\s+)*[A-Za-z_]\w*\s*\**)\s*"
                         r"([A-Za-z_]\w*)\s*(\[[^\]]*\])?\s*;")
    pat_def = re.compile(r"^(?!\s*extern)(?!\s*typedef)(?:static\s+)?(?:const\s+)?(?:volatile\s+)?[A-Za-z_][\w\s\*]*?\b"
                         r"(" + DATA_NAME + r")\s*(?:\[[^\]]*\])*\s*(?:=|;)", re.M)
    files = [p for p in list(Path("src").rglob("*.c")) + list(Path("include").rglob("*.h"))
             if not p.as_posix().startswith(("src/dolphin", "src/trk", "src/crt", "include/dolphin", "include/pc"))]
    for p in files:
        t = p.read_text(errors="replace")
        for m in pat_ext.finditer(t):
            ty = " ".join(m.group(1).replace("const", "").replace("volatile", "").split())
            decls[m.group(2)][ty + ("[]" if m.group(3) else "")] += 1
        # raw tables the PC build leaves out (pcrawdata.py) are generated here, not "defined in C"
        t_defs = re.sub(r"#ifndef TARGET_PC /\* PC: raw table.*?#endif", "", t, flags=re.S)
        for m in pat_def.finditer(t_defs):
            defined.add(m.group(1))
        referenced.update(re.findall(r"\b(" + DATA_NAME + r")\b", t))
    return decls, defined, referenced


def disc_struct_names():
    """Struct tags and typedef names declared `struct DISC_STRUCT` (include/pc/disc_types.h): their fields are big-endian
    views on the PC, so data of such a type is right as the untouched disc bytes (A's idea, 2026-10-15)."""
    names = set()
    for p in list(Path("include").rglob("*.h")) + list(Path("src").rglob("*.c")) + list(Path("src").rglob("*.h")):
        t = p.read_text(errors="replace")
        for m in re.finditer(r"\b(typedef\s+)?struct\s+DISC_STRUCT\s+(\w+)?\s*\{", t):
            if m.group(2):
                names.add(m.group(2))
            if m.group(1):  # typedef: the name after the matching closing brace
                depth, i = 0, m.end() - 1
                while i < len(t):
                    depth += {"{": 1, "}": -1}.get(t[i], 0)
                    if depth == 0:
                        break
                    i += 1
                n = re.match(r"\}\s*(\w+)\s*[;,\[]", t[i:])
                if n:
                    names.add(n.group(1))
    return names


def scalar_struct_layouts():
    """{struct tag / typedef name: [scalar type per field, arrays expanded]} for structs made only of numbers (no
    pointers, no nested structs, no bit-fields): their layout is the same on the GameCube and the PC, so data declared
    with such a type can be converted field by field (2026-10-15 night: many small battle tables are declared as
    `StatusIdTable7` here and `u8[]` there and were left raw big-endian). DISC_STRUCT types are left out (raw by design).
    Two definitions of one name that disagree drop the name."""
    out, bad = {}, set()
    field = re.compile(r"^\s*((?:unsigned\s+|signed\s+)?[A-Za-z_]\w*(?:\s+(?:int|short|char|long))?)\s+(\w+)\s*(?:\[\s*(0x[0-9A-Fa-f]+|\d+)\s*\])?\s*;\s*$")
    for p in list(Path("include").rglob("*.h")) + list(Path("src").rglob("*.c")):
        t = re.sub(r"/\*.*?\*/|//[^\n]*", "", p.read_text(errors="replace"), flags=re.S)
        for m in re.finditer(r"\btypedef\s+struct\s+(?!DISC_STRUCT)(\w+)?\s*\{([^{}]*)\}\s*(\w+)\s*;", t):
            fields, ok = [], True
            for line in m.group(2).split(";"):
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                fm = field.match(line + ";")
                if not fm or ":" in line or fm.group(1) not in SCALARS:
                    ok = False
                    break
                fields += [fm.group(1)] * (int(fm.group(3), 0) if fm.group(3) else 1)
            for name in {m.group(1), m.group(3)} - {None}:
                if not ok or not fields:
                    bad.add(name)
                elif name in out and out[name] != fields:
                    bad.add(name)
                else:
                    out[name] = fields
    padded = {}
    for k, v in out.items():
        if k in bad:
            continue
        fl, off, al = [], 0, 1
        for f in v:  # natural alignment, as both compilers lay these out
            sz = SCALARS[f][0]
            al = max(al, sz)
            while off % sz:
                fl.append("u8")
                off += 1
            fl.append(f)
            off += sz
        while off % al:
            fl.append("u8")
            off += 1
        padded[k] = fl
    return padded


FLOAT_STRUCTS = {"Vec", "Vec3", "GSvec", "GSVec", "Quaternion", "Mtx", "Mtx44", "MtxPtr", "ROMtx", "HSD_Vec3",
                 "CameraVec", "BattleIntroVec", "BillboardVec", "ColorTriple", "HeroMoveVec"}  # all-f32 records


def param_width(ptype):
    """The scalar element type a parameter of this type reads through, or None."""
    t = " ".join(ptype.replace("const", " ").replace("volatile", " ").replace("struct", " ").split())
    t = re.sub(r"\s+\w+$", "", t) if re.search(r"[\w\*]\s+\w+$", t) else t  # drop the parameter name
    if t.rstrip("*").strip() in FLOAT_STRUCTS and (t.endswith("*") or t.strip() in ("Mtx", "MtxPtr", "Mtx44", "ROMtx")):
        return "f32"
    if t.endswith("*") and t.count("*") == 1:
        base = t[:-1].strip()
        if base in SCALARS and SCALARS[base][0] > 1:
            return base
    return None


def function_param_types():
    """{function: [parameter type text, ...]} from include/pc/prototypes.h and one-line definitions in src."""
    out = {}
    pat = re.compile(r"^\s*(?:static\s+|inline\s+|extern\s+)*[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\(([^;{}()]*)\)\s*[;{]?\s*$", re.M)
    srcs = [Path("include/pc/prototypes.h")] + list(Path("src/game").rglob("*.c")) + list(Path("src/hsd").rglob("*.c"))
    for p in srcs:
        t = p.read_text(errors="replace")
        for m in pat.finditer(t):
            ps = [x.strip() for x in m.group(2).split(",")]
            if not ps or ps == [""] or ps == ["void"] or any("..." in x for x in ps):
                continue
            prev = out.get(m.group(1))
            if prev is not None and [param_width(x) for x in prev] != [param_width(x) for x in ps]:
                out[m.group(1)] = None  # declared differently in different places: don't trust it
            elif prev is None and m.group(1) not in out:
                out[m.group(1)] = ps
    return {k: v for k, v in out.items() if v}


def infer_access_widths():
    """{name: element type} for byte-array blobs whose every mention in src/game + src/hsd is a cast read at one scalar
    width, at an index or at a constant offset aligned to that width: `((u32*)X)[i]`, `*(u16*)(X + 0x10)`,
    `*(f32*)&X`. Any other mention (passed to a function, indexed as bytes, mixed widths, a pointer type) leaves the
    blob alone. Conservative on purpose: it only settles tables the code reads one way (2026-10-15)."""
    cast = r"\(\s*(?:const\s+|volatile\s+)*(u16|s16|u32|s32|f32|int|unsigned int|unsigned short|short)\s*\*\s*\)"
    forms = [
        re.compile(r"\(\s*" + cast + r"\s*\(?\s*(" + DATA_NAME + r")\s*\)?\s*\)\s*\["),            # ((T*)X)[i]
        re.compile(r"\*\s*" + cast + r"\s*\(\s*(" + DATA_NAME + r")\s*\+\s*(0x[0-9A-Fa-f]+|\d+)\s*\)"),  # *(T*)(X + c)
        re.compile(r"\*\s*" + cast + r"\s*&?\s*(" + DATA_NAME + r")\b(?!\s*\+)"),                  # *(T*)X, *(T*)&X
    ]
    mentions = collections.Counter()
    good = collections.defaultdict(collections.Counter)
    bad = set()
    params = function_param_types()
    files = list(Path("src/game").rglob("*.c")) + list(Path("src/hsd").rglob("*.c")) + list(Path("src/game").rglob("*.cpp"))
    for p in files:
        t = p.read_text(errors="replace")
        t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
        t = re.sub(r"^\s*extern[^;]*;", " ", t, flags=re.M)  # declarations are not uses
        t = re.sub(r"^#.*$", " ", t, flags=re.M)
        # a blob's own definition is not a use
        t = re.sub(r"^\s*(?:static\s+)?(?:const\s+)?\w+\s+(" + DATA_NAME + r")\s*\[[^\]]*\]\s*(?:__attribute__\(\([^)]*\)\)\s*)?=", " = ",
                   t, flags=re.M)
        # passed whole as an argument to a function whose parameter there points at one scalar width
        for m in re.finditer(r"\b(\w+)\s*\(", t):
            fn = m.group(1)
            if fn not in params:
                continue
            depth, i, start, idx = 1, m.end(), m.end(), 0
            args = []
            while i < len(t) and depth:
                c = t[i]
                if c in "([":
                    depth += 1
                elif c in ")]":
                    depth -= 1
                    if depth == 0:
                        args.append(t[start:i])
                elif c == "," and depth == 1:
                    args.append(t[start:i])
                    start = i + 1
                i += 1
            for k, a in enumerate(args):
                a2 = re.sub(r"\(\s*(?:const\s+)?[\w\s]+\*+\s*\)", "", a).replace("&", "").replace("(", "").replace(")", "").strip()
                if re.fullmatch(DATA_NAME, a2) and k < len(params[fn]):
                    w = param_width(params[fn][k])
                    if w:
                        good[a2][w] += 1
        for n in re.findall(r"\b(" + DATA_NAME + r")\b", t):
            mentions[n] += 1
        for k, f in enumerate(forms):
            for m in f.finditer(t):
                ty, n = m.group(1), m.group(2)
                esz = SCALARS[ty][0]
                if k == 1 and int(m.group(3), 0) % esz:
                    bad.add(n)
                good[n][ty] += 1
    out = {}
    for n, c in good.items():
        sizes = {SCALARS[ty][0] for ty in c}
        kinds = {SCALARS[ty][2] for ty in c}
        if n in bad or len(sizes) != 1 or len(kinds) != 1 or sum(c.values()) != mentions[n]:
            continue
        out[n] = max(c, key=lambda ty: c[ty])
    return out


def read_type_overrides():
    out = {}
    if TYPES_FILE.exists():
        for line in TYPES_FILE.read_text(encoding="utf-8").splitlines():
            line = line.split("#", 1)[0].strip()
            if line:
                name, ty = line.split(None, 1)
                out[name] = ty.strip()
    return out


# ---------------------------------------------------------------- emission
def c_ident(name):
    return re.fullmatch(r"[A-Za-z_]\w*", name) is not None


def align_of(addr, elem):
    a = 1
    while a < 8 and addr % (a * 2) == 0:
        a *= 2
    return max(a, elem)


def c_bytes(data):
    return ", ".join(f"0x{x:02X}" for x in data)


def c_string(data):
    out = []
    for x in data:
        if x == 0x5C:
            out.append("\\\\")
        elif x == 0x22:
            out.append('\\"')
        elif 0x20 <= x < 0x7F:
            out.append(chr(x))
        else:
            out.append(f"\\{x:03o}")
    return '"' + "".join(out) + '"'


def is_string(data):
    if not data or data[-1] != 0:
        return False
    body = data.rstrip(b"\0")
    return len(body) > 0 and all(0x20 <= c < 0x7F or c in (9, 10, 13) or c >= 0x80 for c in body)


def fmt_value(v, kind):
    if kind == "float":
        if v != v:
            # keep the NaN's sign and payload (the math library's lbl_80478AC0 is 0xFFFFFFFF, not the standard NaN);
            # a double NaN with this payload converts back to the same f32 bits when stored into an f32
            bits = struct.unpack(">Q", struct.pack(">d", v))[0]
            payload = bits & ((1 << 51) - 1)
            return ("-" if bits >> 63 else "") + f"__builtin_nan(\"{payload:#x}\")"
        if v in (float("inf"), float("-inf")):
            return "__builtin_inf()" if v > 0 else "-__builtin_inf()"
        return repr(float(v))
    return str(v)


def generate(out_dir, need=None, also_defined=(), link_defined=None, selftest=False):
    symbols = read_symbols()
    decls, defined, referenced = scan_sources()
    if link_defined is not None:  # --link-defined: what the compiled objects really define, not what the C text has
        defined = set(link_defined)
    defined = set(defined) | set(also_defined)  # --defined: symbols another library (port/, musyx-pc) defines itself
    overrides = read_type_overrides()
    objs = {}
    for o in sorted(OBJ_ROOT.rglob("*.o")):
        for name, rec in read_elf(o).items():
            objs.setdefault(name, rec)
    # `lbl_x norelocs` in pcdata_types.txt: dtk's split gave the symbol relocations that are not pointers (numbers that
    # happen to fall inside the code, e.g. a menu-sequence command word taken for __OSDoHotReset+64); harmless on the
    # GameCube (the linker writes back the same value), wrong on the PC (the slot gets a PC address). The objects are
    # RELA (the bytes at the site are zero, the value is target + addend), so write the original GameCube value back
    # into the bytes as a big-endian number and drop the relocation.
    for name in [n for n, t in overrides.items() if t == "norelocs"]:
        del overrides[name]
        if name in objs:
            sname, data, rel, nobits = objs[name]
            data = bytearray(data)
            for ro, t, add, rty in rel:
                # the target's GameCube address: symbols.txt, or the address a local symbol carries in its object
                # name (dtk names local duplicates `<name>_<address>`, e.g. SIInterruptHandler_800CFA60)
                suffix = re.fullmatch(r".*_(8[0-9A-Fa-f]{7})", t)
                addr = symbols[t][1] if t in symbols else int(suffix.group(1), 16) if suffix else None
                if rty != R_PPC_ADDR32 or addr is None or ro + 4 > len(data):
                    raise SystemExit(f"pcdata: {name} norelocs: relocation {t}+{add} at +0x{ro:X} not restorable")
                data[ro:ro + 4] = ((addr + add) & 0xFFFFFFFF).to_bytes(4, "big")
            objs[name] = (sname, bytes(data), [], nobits)
    if need is None:
        need = sorted((n for n in referenced if n in symbols and symbols[n][3] == "object" and n not in defined),
                      key=sort_key)
    # ---- option 1 (port-gaps 22, pcfixed.txt): data at its GameCube address. The raw ranges are copied from main.dol
    # unchanged; symbols inside become absolute (not generated) unless the C reads them natively (a typed declaration)
    # or defines them, which keep a PC copy too. The listed variables become absolute, their start values converted.
    raws, fvars = read_fixed()
    disc_names = disc_struct_names()

    def in_raw(a):
        return any(lo <= a < hi for lo, hi in raws)

    def typed_decl(n):
        for t in decls.get(n, {}):
            base = " ".join(t.replace("[]", "").replace("struct", "").split())
            if base not in BYTE_TYPES and base not in disc_names:
                return True
        return False

    def gc_addr(t):
        suffix = re.fullmatch(r".*_(8[0-9A-Fa-f]{7})", t)
        return symbols[t][1] if t in symbols else int(suffix.group(1), 16) if suffix else None

    fixed, kept = {}, []  # name -> GameCube address (absolute); in a raw range but with a PC copy for the C code
    for n, (_sec, a, _z, kind, _h) in symbols.items():
        if kind == "object" and in_raw(a):
            if n in defined or n in overrides or typed_decl(n):
                kept.append(n)
            else:
                fixed[n] = a
    for n in fvars:
        if n not in symbols or n in defined:
            raise SystemExit(f"pcdata: pcfixed.txt var {n}: " + ("not in symbols.txt" if n not in symbols else
                                                                 "the PC code defines it, so it can't be absolute"))
        fixed[n] = symbols[n][1]
    var_spans = [(symbols[n][1], symbols[n][1] + max(symbols[n][2], 1)) for n in fvars]
    # pointer slots in the copied bytes whose target is outside the fixed data: written with the target's PC address
    # (big-endian, as the scripts read them) at startup
    fixups = []
    for n, (_secname, _data, rel, _nobits) in objs.items():
        a = gc_addr(n)
        if a is None or not in_raw(a):
            continue
        for ro, t, add, rty in rel:
            ta = gc_addr(t)
            if ta is not None and (in_raw(ta + add) or any(lo <= ta + add < hi for lo, hi in var_spans)):
                continue  # main.dol's value is the GameCube address, where the target now is
            if rty != R_PPC_ADDR32 or not c_ident(t):
                raise SystemExit(f"pcdata: {n}: pointer slot at +0x{ro:X} to {t} can't be fixed up")
            fixups.append((a + ro, t, add))
    # closure: data a generated symbol points at (its relocation targets) must exist too, even when no C file names it
    # (strings and tables reached only through other tables)
    need_set = set(need) | {t for _s, t, _a in fixups
                            if t in objs and t not in fixed and symbols.get(t, (0, 0, 0, "object"))[3] == "object"}
    todo = list(need_set)
    while todo:
        n = todo.pop()
        for _off, tname, _add, _ty in (objs[n][2] if n in objs and n not in fixed else []):
            if (tname not in need_set and tname in objs and tname not in defined and
                    symbols.get(tname, (None, 0, 0, "object"))[3] == "object"):
                need_set.add(tname)
                todo.append(tname)
    need = sorted((n for n in need_set if n not in defined), key=sort_key)
    emitted = set(n for n in need if n in objs and c_ident(n) and n not in fixed)
    lines, unsure, relocs, externs, report = [], [], [], {}, collections.Counter()
    # ---- the reference playthrough Part 1 item 2c step 5 (docs/port/disc-data-loading.md, 2026-11-01): no table carries bytes from
    # the disc. Each definition keeps its type, size and alignment; pc_disc_fill() reads its bytes from the player's
    # own main.dol at start-up (pc_disc_check.c, one CRC-32 per table: a check, not data), before the pointer slots
    # are filled. selftest_refs: with --selftest, each table's former initialiser as a static reference copy, compared
    # with the filled table at start-up (pc_data_selftest), the proof that the fill gives the same bytes.
    fills, selftest_refs = [], []
    size_over = {}
    from_disc = Path(__file__).with_name("pcdata_from_disc.txt")
    if from_disc.exists():  # NAME [SIZE]: SIZE raises a table's size (a table that runs on past the next symbol)
        for raw_line in from_disc.read_text(encoding="utf-8").splitlines():
            parts = raw_line.split("#", 1)[0].split()
            if len(parts) > 1:
                size_over[parts[0]] = int(parts[1], 0)

    def disc_crc(name, addr, size):
        d = dol_bytes(addr, size)
        if d is None:
            raise SystemExit(f"pcdata: {name} (0x{addr:08X} +0x{size:X}) is not all in main.dol's file")
        return zlib.crc32(d)

    def from_disc_def(decl, name, addr, size, esz, old_line):
        """decl: the definition without initialiser; esz 1 = copy, 2 / 4 / 8 = swap each element"""
        lines.append(decl + " /* from the player's main.dol at start-up */")
        if esz == 1:
            fills.append(f"    pc_disc_dol_read({name_ref(name)}, 0x{addr:08X}u, 0x{size:X}u, 0x{disc_crc(name, addr, size):08X}u);")
        else:
            fills.append(f"    pc_disc_dol_read_swapped({name_ref(name)}, 0x{addr:08X}u, 0x{size:X}u, {esz}u, "
                         f"0x{disc_crc(name, addr, size):08X}u);")
        if old_line is not None:
            selftest_refs.append((name, old_line))
        report["filled from main.dol at start-up"] += 1

    def name_ref(name):
        return f"(void*)&{name}"

    disc_types = disc_struct_names()
    scalar_structs = scalar_struct_layouts()
    access_widths = infer_access_widths()
    settled = []  # (name, size, why): raw bytes that are right by design, not unsure

    def disc_typed(tys):
        """Every declaration is a DISC_STRUCT type (or array of one) or a plain byte array, and at least one is a
        DISC_STRUCT type: the code reads the bytes only through big-endian views (or byte by byte)."""
        kinds = set()
        for t in tys:
            base = " ".join(t.replace("[]", "").replace("struct", "").split())
            if base in disc_types:
                kinds.add("disc")
            elif base in ("u8", "s8", "char", "unsigned char", "signed char"):
                kinds.add("byte")
            else:
                return False
        return "disc" in kinds

    def target_expr(tname, add):
        if not c_ident(tname):
            return None
        if tname not in emitted:
            kind = symbols.get(tname, (None, 0, 0, "object"))[3]
            externs[tname] = "extern void %s(void);" % tname if kind == "function" else "extern char %s[];" % tname
        return f"((char*)&{tname} + {add})" if add else f"(void*)&{tname}"

    # pcblocks.txt: an inner label of a kept block is never generated on its own (firstlink aliases it to block +
    # offset); the block itself is generated instead
    inner = {n: b for n, b, _ in BLOCK_ALIASES}
    need = list(dict.fromkeys(inner.get(n, n) for n in need))
    for name in need:
        if name in fixed:
            # option 1: at its GameCube address (the linker defines it there, firstlink --defsym; the bytes are copied
            # by pc_fixed_copy below)
            externs[name] = f"extern char {name}[];"
            report["at its GameCube address (pcfixed.txt)"] += 1
            continue
        if name not in objs:
            unsure.append((name, "-", "no original bytes found in build/GC6E01/obj (not in main.dol?)"))
            report["missing source bytes"] += 1
            continue
        if not c_ident(name):
            unsure.append((name, "-", "name is not a C identifier"))
            report["not a C name"] += 1
            continue
        secname, data, rel, nobits = objs[name]
        sec, addr, size, kind, hint = symbols.get(name, (secname, 0, len(data), "object", None))
        if nobits and name in KEPT_BLOCKS:  # pcblocks.txt: the whole zero-filled block, not the object's first label
            data = bytes(size)
        size = len(data)
        # *Changed 2026-11-04 (port-gaps 61's class audit):* pcdata_from_disc.txt's SIZE applies on every path, not
        # only the raw one: the e-Reader link's port tables lbl_80478950 / lbl_80478954 are dtk-split 1-byte "byte"
        # symbols, which took the typed u8 path at size 1, so [1]-[3] read 0 on the PC
        if size_over.get(name, 0) > size and not nobits and name not in KEPT_BLOCKS:
            longer = dol_bytes(addr, size_over[name])
            if longer is None:
                raise SystemExit(f"pcdata: {name} (0x{addr:08X} +0x{size_over[name]:X}) is not all in main.dol's file")
            data, size = longer, len(longer)
        if overrides.get(name) == "bytes" and not rel and not nobits:
            # `lbl_x bytes` (2026-10-18, datawidth.py triage): the code reads this table byte by byte, even where it
            # copies it in wider pieces first (a u32 copy into a local byte buffer, a u16* pointer to {numerator,
            # denominator} byte pairs). The disc's bytes are the right PC bytes: no conversion, and not unsure.
            decl = f"u8 {name}[{size}] __attribute__((aligned({max(align_of(addr, 1), 4)})))"
            from_disc_def(decl + ";", name, addr, size, 1, f"{decl} = {{{c_bytes(data)}}};")
            settled.append((name, f"0x{size:X}", "pcdata_types.txt: bytes"))
            report["raw by design (pcdata_types.txt bytes)"] += 1
            continue
        tys = decls.get(name, {})
        ty = overrides.get(name) or (next(iter(tys)) if len(tys) == 1 else None)
        reason = None
        if name not in overrides and len(tys) > 1:
            # Declarations that differ only within one element size (u32 / s32 / f32, u8[] / char[]) give the same
            # bytes after the byte swap, which depends on the element size alone: convert them, using the most common
            # declaration. Different element sizes are a real conflict: unsure.
            bases = {t[:-2].strip() if t.endswith("[]") else t for t in tys}
            sizes = {SCALARS[b][0] for b in bases if b in SCALARS}
            if all(b in SCALARS for b in bases) and len(sizes) == 1:
                ty = max(tys, key=lambda t: tys[t])
                if any(t.endswith("[]") for t in tys) and not ty.endswith("[]"):
                    ty += "[]"
                report["conflicting declarations, same element size (converted)"] += 1
            else:
                # A byte-array declaration (`extern u8 x[]`) only says "some bytes"; a wide scalar declaration says how
                # the code reads them. When the only disagreement is wide scalar vs byte array, convert by the wide type
                # (B's boot, handover 28: main.c's u32 memory-range table lbl_802663A0 was left raw and big-endian).
                # Pointer declarations stay a real conflict (4 bytes on the GameCube, 8 on the PC).
                wide = {t: c for t, c in tys.items()
                        if (t[:-2].strip() if t.endswith("[]") else t) not in BYTE_TYPES}
                wbases = {t[:-2].strip() if t.endswith("[]") else t for t in wide}
                wsizes = {SCALARS[b][0] for b in wbases if b in SCALARS}
                if wide and len(wide) < len(tys) and all(b in SCALARS for b in wbases) and len(wsizes) == 1:
                    ty = max(wide, key=lambda t: wide[t])
                    if not ty.endswith("[]") and (any(t.endswith("[]") for t in tys) or size > SCALARS[ty][0]):
                        ty += "[]"
                    report["wide declaration over byte-array declarations (converted)"] += 1
                elif (size == 4 and not any(t.endswith("[]") for t in tys)
                      and all(t.endswith("*") or SCALARS.get(t, (0,))[0] == 4 and SCALARS[t][2] == "int" for t in tys)
                      and any(t.endswith("*") for t in tys)):
                    # One 4-byte slot declared as an integer here and a pointer there: emit a real pointer. On the
                    # little-endian PC a u32 reader sees its low half (the value, under the low-4 GB layout) and a
                    # u32 writer leaves the upper half zero; a 4-byte slot would hand pointer readers garbage.
                    ty = "void*"
                    report["one slot, integer vs pointer declarations (pointer)"] += 1
                elif (len({t.replace("struct ", "").replace("[]", "").strip() for t in tys
                           if t.replace("[]", "").strip() not in BYTE_TYPES}) == 1
                      and next(iter({t.replace("struct ", "").replace("[]", "").strip() for t in tys
                                     if t.replace("[]", "").strip() not in BYTE_TYPES})) in scalar_structs):
                    # a numbers-only struct here, "some bytes" (u8[]) there: convert by the struct's fields
                    sname = next(iter({t.replace("struct ", "").replace("[]", "").strip() for t in tys
                                       if t.replace("[]", "").strip() not in BYTE_TYPES}))
                    ty = "{" + ", ".join(scalar_structs[sname]) + "}[]"
                    report["numbers-only struct over byte-array declarations (converted per field)"] += 1
                else:
                    reason = "conflicting declarations: " + ", ".join(sorted(tys))
        is_array = bool(ty and ty.endswith("[]"))
        base = ty[:-2].strip() if is_array else ty
        al = align_of(addr, 1)
        # ---- zero-initialised data (.bss / .sbss)
        if nobits:
            # pcblocks.txt: a work area of several variables, possibly pointers: reserved x2, as before the merge
            in_block = name in KEPT_BLOCKS
            if base in SCALARS and not reason and not in_block:
                esz = SCALARS[base][0]
                n = max(1, size // esz)
                lines.append(f"{base} {name}{'[%d]' % n if (is_array or n > 1) else ''} __attribute__((aligned({max(al, esz)})));")
                report["zero-initialised, typed"] += 1
            elif base and base.endswith("*") and not is_array and size == 4 and not reason and not in_block:
                # one zero pointer slot: 8 zero bytes are exactly its PC form (not a guess, so not "unsure")
                lines.append(f"u8 {name}[8] __attribute__((aligned(8))); /* one pointer slot, zero */")
                report["zero-initialised, one pointer slot"] += 1
            else:
                # struct or unknown type: the PC layout may be larger (pointer members): reserve twice the size
                lines.append(f"u8 {name}[{max(size * 2, 8)}] __attribute__((aligned(8))); /* GameCube size 0x{size:X}, reserved x2 */")
                unsure.append((name, f"0x{size:X}", reason or f"zero data of type {ty or 'unknown'}: PC size unknown, reserved 2x"))
                report["zero-initialised, unsure"] += 1
            continue
        # ---- a record array from the override file: `{s32, void*}[]` (B's boot run 35: the fsys status dispatch table
        # lbl_8036C3E0, {status, handler} pairs, was left raw while the code walked it as native structs). Each GameCube
        # record is converted field by field: numbers to native order, pointer words through their relocations; the
        # PC array has the native layout (8-byte pointers), as the C structs that read it expect.
        if ty and ty.endswith("[]") and ty[:-2].replace("struct ", "").strip() in scalar_structs:
            # an override naming a numbers-only struct (pcdata_types.txt `lbl_x PokemonGrowData[]`): its fields
            ty = "{" + ", ".join(scalar_structs[ty[:-2].replace("struct ", "").strip()]) + "}[]"
        rec = re.fullmatch(r"\{([^}]*)\}\[\]", ty or "")
        if rec and not nobits and not reason:
            # `T*N` repeats a field N times (2026-10-18: `{u16*20, u32*38}[]`); `void*` stays a pointer field
            fields = []
            for f in (f.strip() for f in rec.group(1).split(",")):
                rep = re.fullmatch(r"(.+?)\s*\*\s*(\d+)", f)
                fields += [rep.group(1).strip()] * int(rep.group(2)) if rep else [f]
            # `p32`: a pointer kept as a 4-byte GameCube cell (the PC record keeps the GameCube layout, for C views that
            # read it at GameCube offsets with PC_PTR); its relocation becomes a startup relocation, as in u32 tables
            # (2026-10-17: the summary / item menu page table lbl_80266918 and its action lists)
            gc_sizes = [4 if f.endswith("*") or f == "p32" else SCALARS.get(f, (0,))[0] for f in fields]
            rsize = sum(gc_sizes)
            tail = size % rsize if rsize else 0
            if 0 in gc_sizes or not rsize or size < rsize or (tail and any(data[size - tail:])):
                reason = f"record type {ty}: unknown field or size 0x{size:X} not a multiple of 0x{rsize:X}"
            elif any(x[0] >= size - tail for x in rel) and tail:
                reason = f"record type {ty}: relocation in the tail"
            else:
                # a zero tail shorter than one record is alignment padding after the last record
                recs, bad = [], None
                p32_relocs, ptr_fields = [], []  # ptr_fields: (record, field index, target expression)
                for r in range(size // rsize):
                    vals, off = [], r * rsize
                    for fi, (f, fs) in enumerate(zip(fields, gc_sizes)):
                        rr = [x for x in rel if x[0] == off]
                        if f == "p32":
                            w = struct.unpack(">I", data[off:off + 4])[0]
                            if rr:
                                expr = target_expr(rr[0][1], rr[0][2])
                                if expr is None or rr[0][3] != R_PPC_ADDR32:
                                    bad = f"relocation to {rr[0][1]} not resolvable"
                                else:
                                    p32_relocs.append((name, off, expr, False))
                                vals.append("0")
                            elif w:
                                bad = f"non-zero pointer word 0x{w:08X} without a relocation"
                            else:
                                vals.append("0")
                        elif f.endswith("*"):
                            w = struct.unpack(">I", data[off:off + 4])[0]
                            if rr:
                                expr = target_expr(rr[0][1], rr[0][2])
                                if expr is None or rr[0][3] != R_PPC_ADDR32:
                                    bad = f"relocation to {rr[0][1]} not resolvable"
                                vals.append(f"(void*){expr}" if expr else "0")
                                if expr:
                                    ptr_fields.append((r, fi, expr))
                            elif w:
                                bad = f"non-zero pointer word 0x{w:08X} without a relocation"
                            else:
                                vals.append("0")
                        else:
                            if rr:
                                bad = f"relocation inside the number field at +0x{off:X}"
                            v = struct.unpack(">" + SCALARS[f][1], data[off:off + fs])[0]
                            vals.append(fmt_value(v, SCALARS[f][2]))
                        off += fs
                    recs.append("{" + ", ".join(vals) + "}")
                if bad:
                    reason = bad
                else:
                    relocs.extend(p32_relocs)
                    members = " ".join(f"{'void*' if f.endswith('*') else 'u32' if f == 'p32' else f} f{i};"
                                       for i, f in enumerate(fields))
                    # step 5: a named record type (the compiler gives the PC field offsets), no initialiser; the number
                    # fields are filled from main.dol (pc_disc_fill_records), the pointer fields by the slot table
                    sname = f"pcrec_{name}"
                    lines.append(f"struct {sname} {{ {members} }};")
                    lines.append(f"struct {sname} {name}[{len(recs)}] __attribute__((aligned(8))); "
                                 f"/* from the player's main.dol at start-up */")
                    gc_offs = [sum(gc_sizes[:i]) for i in range(len(fields))]
                    numbers = [(i, gc_offs[i], gc_sizes[i]) for i, f in enumerate(fields)
                               if not f.endswith("*") and f != "p32"]
                    lines.append(f"static const PcDiscField pcff_{name}[] = {{" +
                                 ", ".join(f"{{offsetof(struct {sname}, f{i}), {go}u, {gs}u}}" for i, go, gs in numbers)
                                 + ("" if numbers else "{0, 0, 0}") + "};")
                    if numbers:
                        fills.append(f"    pc_disc_fill_records({name}, sizeof({name}[0]), pcff_{name}, {len(numbers)}u, "
                                     f"0x{addr:08X}u, {rsize}u, {len(recs)}u, "
                                     f"0x{disc_crc(name, addr, rsize * len(recs)):08X}u);")
                    for r, fi, expr in ptr_fields:
                        relocs.append((name, f"{r} * sizeof({name}[0]) + offsetof(struct {sname}, f{fi})", expr, False))
                    selftest_refs.append((name, f"static const struct {sname} {name}__ref[{len(recs)}] = "
                                                f"{{{', '.join(recs)}}};"))
                    report["record array from an override (converted per field)"] += 1
                    report["filled from main.dol at start-up"] += 1
                    continue
        # ---- initialised data
        if (base in BYTE_TYPES and name in access_widths and not rel and not reason and name not in overrides
                and size % SCALARS[access_widths[name]][0] == 0 and not is_string(data)):
            base = access_widths[name]
            ty, is_array = base + "[]", True
            report["byte blob the code reads one way (converted by access width)"] += 1
        hint_size = HINT_SIZE.get(hint or "", None)
        if base in SCALARS and not reason:
            esz, fmt, k = SCALARS[base]
            agree = hint is None or hint in ("string", "wstring") and esz == 1 or hint_size in (None, esz) or \
                (hint == "float" and k == "float") or (hint == "double" and k == "float" and esz == 8)
            # an explicit f32[] override wins over dtk's "4byte" hint (A, 2026-10-17: six float tables, e.g. the menu
            # model's lights lbl_80271F38, stayed raw big-endian and drew near-black)
            if k == "float" and hint not in (None, "float", "double") and name not in overrides:
                agree = False
            # any explicit override wins over dtk's size hint too (2026-10-18: dtk takes its hint from the first access
            # it sees, which for a table copied in u32 pieces is "4byte" although the code reads u16 elements, e.g.
            # Pickup's item table lbl_8027296C)
            if name in overrides:
                agree = True
            if size % esz or not agree or (rel and not (esz == 4 and k == "int")):
                reason = f"type {ty} disagrees with size 0x{size:X} / dtk hint {hint} / relocations"
            else:
                n = size // esz
                if esz == 1 and not rel and (hint == "string" or is_string(data)):
                    # text: byte order doesn't apply; the bytes are copied as they are (step 5: from main.dol, and the
                    # text is no longer echoed in a comment)
                    decl = f"{base} {name}[{n}] __attribute__((aligned({al})))"
                    from_disc_def(decl + ";", name, addr, size, 1, f"{decl} = {{{c_bytes(data)}}};")
                    report["string"] += 1
                    continue
                if esz == 1 and not rel and hint not in ("byte",):
                    # a byte array whose accesses we can't prove are byte-wide: raw bytes, unsure
                    reason = "byte array, accesses not proven byte-wide"
                else:
                    vals = list(struct.unpack(">" + fmt * n, data))
                    for ro, t, add, rty in rel:
                        if rty != R_PPC_ADDR32 or ro % 4:
                            reason = f"relocation type {rty} at +0x{ro:X}"
                            break
                        expr = target_expr(t, add)
                        if expr is None:
                            reason = f"relocation to non-C name {t}"
                            break
                        vals[ro // 4] = 0
                        relocs.append((name, ro, expr, False))
                    if not reason:
                        body = ", ".join(fmt_value(v, k) for v in vals)
                        if n == 1 and not is_array:
                            decl = f"{base} {name} __attribute__((aligned({max(al, esz)})))"
                            old = f"{decl} = {body};"
                        else:
                            decl = f"{base} {name}[{n}] __attribute__((aligned({max(al, esz)})))"
                            old = f"{decl} = {{{body}}};"
                        # step 5: the elements from main.dol, swapped to the PC's order; pointer slots (zero in the old
                        # initialiser) are overwritten by the slot table afterwards
                        from_disc_def(decl + ";", name, addr, size, esz, old)
                        report["typed, converted" + (" (with pointer slots)" if rel else "")] += 1
                        continue
        elif base and base.endswith("*") and not reason:
            if size % 4:
                reason = f"pointer type with size 0x{size:X}"
            else:
                n = size // 4
                words = struct.unpack(">" + "I" * n, data)
                slots, bad = ["0"] * n, None
                for i, w in enumerate(words):
                    r = [x for x in rel if x[0] == 4 * i]
                    if r:
                        expr = target_expr(r[0][1], r[0][2])
                        if expr is None or r[0][3] != R_PPC_ADDR32:
                            bad = f"relocation to {r[0][1]} not resolvable"
                            break
                        slots[i] = expr
                    elif w != 0:
                        bad = f"non-zero pointer word 0x{w:08X} without a relocation"
                        break
                if bad:
                    reason = bad
                else:
                    if n == 1 and not is_array:
                        lines.append(f"void* {name} = {slots[0]};")
                    else:
                        lines.append(f"void* {name}[{n}] = {{{', '.join(slots)}}};")
                    report["pointers, converted"] += 1
                    continue
        if reason is None:
            reason = f"type {ty or 'unknown'} (struct or unrecognised): layout not known"
        # ---- unsure: untouched original bytes (big-endian), pointer words through the relocation table
        for ro, t, add, rty in rel:
            # any alignment: the big-endian slot is written byte by byte (the battle move scripts carry their jump
            # targets at odd offsets inside the byte stream, e.g. a command byte then a 4-byte target)
            expr = target_expr(t, add) if rty == R_PPC_ADDR32 else None
            if expr is not None:
                relocs.append((name, ro, expr, True))
        if name not in overrides and tys and disc_typed(tys):
            # settled: a DISC_STRUCT type reads these bytes big-endian on the PC, pointer slots included (DISC_PTR)
            decl = f"u8 {name}[{size}] __attribute__((aligned({max(al, 4)})))"
            from_disc_def(decl + "; /* raw big-endian: DISC_STRUCT data */", name, addr, size, 1,
                          f"{decl} = {{{c_bytes(data)}}};")
            settled.append((name, f"0x{size:X}", "declared as " + ", ".join(sorted(tys))))
            report["raw by design (DISC_STRUCT type)"] += 1
            continue
        # step 5: raw from main.dol; pcdata_from_disc.txt may raise the size (a table that runs on past the next symbol)
        raw_size = max(size, size_over.get(name, 0))
        decl = f"u8 {name}[{raw_size}] __attribute__((aligned({max(al, 4)})))"
        from_disc_def(decl + "; /* unsure: raw big-endian */", name, addr, raw_size, 1,
                      None if raw_size != size else f"{decl} = {{{c_bytes(data)}}};")
        unsure.append((name, f"0x{size:X}", reason + (f"; {len(rel)} pointer slot(s) relocated big-endian" if rel else "")))
        report["unsure (raw bytes)"] += 1

    # ---- option 1: the bytes for the fixed data, copied to their GameCube addresses before the pointer slots are filled.
    # Since 2026-11-01 (the reference playthrough Part 1 item 2c step 2, docs/port/disc-data-loading.md) they come from the player's own
    # main.dol at start-up (pc_disc_check.c), not from bytes in the exe: only each range's address, size and CRC-32 (a
    # check that the right bytes arrived, not the data) are generated.
    fixed_lines = ["void pc_disc_dol_copy(u32 addr, u32 size, u32 crc); /* pc_disc_check.c: from the disc's main.dol */",
                   "void pc_disc_dol_copy_swapped(u32 addr, u32 size, u32 esz, u32 crc);"]
    copy_lines = []
    for i, (lo, hi) in enumerate(raws):
        raw = dol_bytes(lo, hi - lo)
        if raw is None:
            raise SystemExit(f"pcdata: pcfixed.txt raw 0x{lo:08X}-0x{hi:08X} is not all in main.dol's file")
        copy_lines.append(f"    pc_disc_dol_copy(0x{lo:08X}u, 0x{hi - lo:X}u, 0x{zlib.crc32(raw):08X}u);")
    for n, ty in sorted(fvars.items(), key=lambda kv: sort_key(kv[0])):
        _sec, a, z = symbols[n][0], symbols[n][1], max(symbols[n][2], 1)
        raw = dol_bytes(a, z)
        if raw is None:  # .sbss: zero at start
            copy_lines.append(f"    __builtin_memset((void*)(uintptr_t)0x{a:08X}u, 0, {z}); /* {n} */")
            continue
        esz = {"u8": 1, "u16": 2, "u32": 4}[ty]
        if z % esz:
            raise SystemExit(f"pcdata: pcfixed.txt var {n}: size 0x{z:X} is not a multiple of {ty}")
        copy_lines.append(f"    pc_disc_dol_copy_swapped(0x{a:08X}u, {z}u, {esz}u, 0x{zlib.crc32(raw):08X}u); /* {n}, {ty} */")
    for slot, t, add in fixups:
        expr = target_expr(t, add)
        relocs.append((f"(char*)(uintptr_t)0x{slot:08X}u", 0, expr, True))
    report["pointer slots in fixed data fixed up at startup"] = len(fixups)
    report["in a fixed range, PC copy kept for the C code"] = len(kept)

    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / "fixed_symbols.txt").write_text(
        f"# {MARKER}: absolute symbols at their GameCube address (pcfixed.txt); firstlink passes them to --defsym\n"
        + "".join(f"{n} 0x{a:08X}\n" for n, a in sorted(fixed.items())), encoding="utf-8")
    (out / "pc_block_aliases.txt").write_text(  # pcblocks.txt: inner labels of a kept block (firstlink's aliases)
        "".join(f"{n} = {b} + 0x{off:X}\n" for n, b, off in sorted(BLOCK_ALIASES)), encoding="utf-8")
    report["inner labels of a kept block (pcblocks.txt), aliased"] = len(BLOCK_ALIASES)
    for n in size_over:
        if n not in symbols:
            raise SystemExit(f"pcdata: pcdata_from_disc.txt: {n} is not in symbols.txt")
    head = [f"/* {MARKER}: generated by tools/local/port/pcdata.py from the original program's data. Game data from the",
            " * player's disc: never commit (the git hooks in tools/local/hooks refuse it). Regenerate instead. */",
            "#include <stddef.h>",
            "#include <stdint.h>",
            "typedef uint8_t u8; typedef int8_t s8; typedef uint16_t u16; typedef int16_t s16; typedef uint32_t u32;",
            "typedef int32_t s32; typedef uint64_t u64; typedef int64_t s64; typedef float f32; typedef double f64;",
            "typedef int BOOL;",
            "/* pc_disc_check.c: main.dol's bytes by GameCube address (the reference playthrough Part 1 item 2c step 5) */",
            "typedef struct PcDiscField { u32 pc_offset, gc_offset, size; } PcDiscField;",
            "void pc_disc_dol_read(void* dst, u32 addr, u32 size, u32 crc);",
            "void pc_disc_dol_read_swapped(void* dst, u32 addr, u32 size, u32 esz, u32 crc);",
            "void pc_disc_fill_records(void* dst, u32 pc_record_size, const PcDiscField* fields, u32 field_count,",
            "                          u32 addr, u32 gc_record_size, u32 record_count, u32 crc);", ""]
    # forward declarations of everything defined below, so a table may point at a symbol defined later in the file
    forward = []
    for ln in lines:
        m = re.match(r"^(?!static\b)((?:const\s+)?[A-Za-z_][\w\s\*]*?)\s*\b([A-Za-z_]\w*)\s*(\[[^\]]*\])?"
                     r"(?:\s*__attribute__\(\([^=;]*\)\))?\s*(?:=|;)", ln)
        if m and not ln.startswith(("extern", "typedef", "struct", "#")):
            forward.append(f"extern {' '.join(m.group(1).split())} {m.group(2)}{'[]' if m.group(3) else ''}; /* defined below */")
    defined_here = {re.search(r"(\w+)(?:\[\])?; /\* defined below \*/$", f).group(1) for f in forward}
    for n in list(externs):
        if n in defined_here:
            del externs[n]  # defined in this file (its forward declaration has the real type)
    body = head + sorted(externs.values()) + [""] + forward + [""] + lines + [""]
    body += ["/* Pointer slots in GameCube-layout data: filled once at startup with the target's PC address (below 4 GB",
             " * in OrrePort's fixed layout; the project notes 6.3). big = 1: the slot is stored big-endian. */",
             "#include <stdio.h>", "#include <stdlib.h>",
             "struct pc_data_reloc { void* slot; void* target; int big; };",
             "struct pc_data_reloc pc_data_relocs[] = {"]
    body += [f"    {{(char*){n} + {ro}, {expr}, {1 if big else 0}}}," for n, ro, expr, big in relocs] or ["    {0, 0, 0},"]
    body += ["};",
             "const unsigned pc_data_reloc_count = sizeof(pc_data_relocs) / sizeof(pc_data_relocs[0]);",
             "/* option 1 (port-gaps 22, tools/local/port/pcfixed.txt): the battle scripts, their tables and the",
             " * variables they address, at their GameCube addresses in MEM1 (below the heap arena) */"]
    body += fixed_lines
    body += ["/* step 5: every table's bytes from the player's main.dol (one CRC-32 per table checks they arrived) */",
             "static void pc_disc_fill(void)", "{"] + fills + ["}"]
    selftest_call = []
    if selftest:
        # --selftest: the former initialisers as reference copies, compared with the filled and relocated tables (the
        # pointer slots of number and raw tables are skipped: the reference holds 0 / the GameCube address there)
        masks = collections.defaultdict(list)
        for n, ro, _expr, _big in relocs:
            if isinstance(ro, int):
                masks[n].append(ro)
        body += ["#include <string.h>", "/* --selftest reference copies (2026-11-01) */"]
        for n, old in selftest_refs:
            ref = old if old.startswith("static const") else "static const " + old.replace(f" {n}", f" {n}__ref", 1)
            body.append(ref)
        body += ["static int pc_selftest_cmp(const char* name, const void* live, const void* ref, size_t size,",
                 "                           const unsigned* skip, unsigned nskip)",
                 "{",
                 "    const unsigned char* a = (const unsigned char*)live; const unsigned char* b = (const unsigned char*)ref;",
                 "    for (size_t i = 0; i < size; i++) {",
                 "        int skipped = 0;",
                 "        for (unsigned k = 0; k < nskip; k++) if (i >= skip[k] && i < skip[k] + 4) skipped = 1;",
                 "        if (!skipped && a[i] != b[i]) {",
                 "            fprintf(stderr, \"[pc_data selftest] %s differs at +0x%zX (filled %02X, reference %02X)\\n\",",
                 "                    name, i, a[i], b[i]);",
                 "            return 1;",
                 "        }",
                 "    }",
                 "    return 0;",
                 "}",
                 "static void pc_data_selftest(void)",
                 "{",
                 "    int bad = 0, n = 0;"]
        for n, _old in selftest_refs:
            sk = masks.get(n, [])
            arr = f"(const unsigned[]){{{', '.join(str(x) for x in sk)}}}" if sk else "NULL"
            body.append(f"    bad += pc_selftest_cmp(\"{n}\", (const void*)&{n}, (const void*)&{n}__ref, "
                        f"sizeof({n}__ref), {arr}, {len(sk)}u); n++;")
        body += ["    fprintf(stderr, \"[pc_data selftest] %d of %d tables differ from the former initialisers\\n\", bad, n);",
                 "    if (bad) abort();",
                 "}"]
        selftest_call = ["    pc_data_selftest(); /* --selftest build only */"]
    body += ["static void pc_fixed_copy(void)", "{"] + copy_lines + ["}",
             "void pc_data_relocate(void)",
             "{",
             "    pc_disc_fill(); /* every table from the player's main.dol (step 5) */",
             "    pc_fixed_copy();",
             "    for (size_t i = 0; i < sizeof(pc_data_relocs) / sizeof(pc_data_relocs[0]); i++) {",
             "        struct pc_data_reloc* r = &pc_data_relocs[i];",
             "        uintptr_t v = (uintptr_t)r->target;",
             "        unsigned char* s = (unsigned char*)r->slot;",
             "        if (s == NULL) continue;",
             "        if (v >> 32) { fprintf(stderr, \"pc_data_relocate: target above 4 GB (%p)\\n\", r->target); abort(); }",
             "        if (r->big) { s[0] = (u8)(v >> 24); s[1] = (u8)(v >> 16); s[2] = (u8)(v >> 8); s[3] = (u8)v; }",
             "        else { u32 w = (u32)v; __builtin_memcpy(s, &w, 4); }",
             "    }"] + selftest_call + ["}", ""]
    (out / "pc_data_main.c").write_text("\n".join(body), encoding="utf-8")
    (out / "unsure_data.txt").write_text(f"# {MARKER}: unsure data symbols (raw original bytes), checked first in Dolphin comparisons\n"
                                         + "".join(f"{n}\t{s}\t{r}\n" for n, s, r in unsure), encoding="utf-8")
    rep = [f"needed: {len(need)}"] + [f"{k}: {v}" for k, v in sorted(report.items())] + [f"pointer slots relocated at startup: {len(relocs)}"]
    (out / "pcdata_report.txt").write_text(f"# {MARKER}\n" + "\n".join(rep) + "\n", encoding="utf-8")
    print("\n".join(rep))
    print(f"-> {out / 'pc_data_main.c'}")


if __name__ == "__main__":
    need = None
    if "--need" in sys.argv:
        need = [l.strip() for l in Path(sys.argv[sys.argv.index("--need") + 1]).read_text().splitlines() if l.strip()]
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else "build-pc/generated"
    also = []
    if "--defined" in sys.argv:  # one symbol per line: data the PC build defines elsewhere with real types (B's libraries)
        also = [l.strip() for l in Path(sys.argv[sys.argv.index("--defined") + 1]).read_text().splitlines()
                if l.strip() and not l.startswith("#")]
    linked = None
    if "--link-defined" in sys.argv:  # one symbol per line: names the compiled PC game objects define (nm)
        linked = [l.strip() for l in Path(sys.argv[sys.argv.index("--link-defined") + 1]).read_text().splitlines()
                  if l.strip() and not l.startswith("#")]
    if need is not None:
        # The room modules' setup stores and pointers name main-program data (lbl_804791D0 ...) that no compiled unit
        # uses, so a link without the generated data (firstlink's need list) never asks for it: add what pcreldata.py
        # references.
        import contextlib
        import io
        sys.path.insert(0, str(Path(__file__).parent))
        import pcreldata
        with contextlib.redirect_stdout(io.StringIO()):
            rel_refs = pcreldata.generate(out)
        need = sorted(set(need) | set(rel_refs), key=sort_key)
    # --selftest (or ORRE_PCDATA_SELFTEST=1, so a firstlink run can ask for it): reference copies + pc_data_selftest()
    import os
    generate(out, need, also, linked, selftest="--selftest" in sys.argv or os.environ.get("ORRE_PCDATA_SELFTEST") == "1")
