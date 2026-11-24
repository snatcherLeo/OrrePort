"""One definition per symbol for the PC link (OrrePort first link; port-gaps.md).

The GameCube build compiles many C files into more than one unit: "candidate" units (configure.py CodeCandidate /
DataCandidate) are compiled only to be compared and are never linked, and multi-unit files (`gs_title.c`,
`gba_misc.c`, `fight_menu.c`, ...) are included by several units with different defines. So the same function or
variable is often *defined* in several objects. The GameCube link takes exactly one copy (or the original
assembly); the PC link has to choose too, or it fails with multiple definitions.

Rule: a symbol belongs to the unit whose split range (config/GC6E01/splits.txt) contains its address
(config/GC6E01/symbols.txt). In every other object its definition is made weak, so the owner's copy wins; if the owner
isn't compiled for the PC at all, the weak copies still provide one (the first one the linker sees).

Usage (repo root):
  python -I tools/local/port/pcunits.py report            # how many symbols are defined in more than one GameCube
                                                            # object (build/GC6E01/src/**.o), and by which units
  python -I tools/local/port/pcunits.py weaken OBJ UNIT     # for one PC object compiled from UNIT (its source path as
                                                            # in splits.txt, e.g. game/gs_title_80023DA8.c): write the
                                                            # list of its defined globals owned by another unit, for
                                                            # `objcopy --weaken-symbols=LIST OBJ`
"""
import bisect
import collections
import re
import struct
import subprocess
import sys
from pathlib import Path


def split_ranges():
    """Sorted [(start, end, unit source path)] over all sections of the main program."""
    out, unit = [], None
    for line in Path("config/GC6E01/splits.txt").read_text(encoding="utf-8").splitlines():
        if line and not line[0].isspace() and line.endswith(":") and line != "Sections:":
            unit = line[:-1]
            continue
        m = re.match(r"\s+\.\w+\s+start:0x([0-9A-Fa-f]+) end:0x([0-9A-Fa-f]+)", line)
        if m and unit:
            out.append((int(m.group(1), 16), int(m.group(2), 16), unit))
    out.sort()
    return out


def owners():
    """symbol name -> owning unit (source path as in splits.txt), for every symbol in symbols.txt with an address."""
    ranges = split_ranges()
    starts = [r[0] for r in ranges]
    out = {}
    for line in Path("config/GC6E01/symbols.txt").read_text(encoding="utf-8").splitlines():
        m = re.match(r"(\S+) = \.\w+:0x([0-9A-Fa-f]+);", line)
        if not m:
            continue
        a = int(m.group(2), 16)
        i = bisect.bisect_right(starts, a) - 1
        if i >= 0 and ranges[i][0] <= a < ranges[i][1]:
            out[m.group(1)] = ranges[i][2]
    return out


def elf_defined_globals(path):
    """Names of global or weak symbols defined in an ELF object (either byte order)."""
    b = Path(path).read_bytes()
    if b[:4] != b"\x7fELF":
        return []
    e = ">" if b[5] == 2 else "<"
    is64 = b[4] == 2
    if is64:
        shoff, = struct.unpack(e + "Q", b[0x28:0x30])
        shentsize, shnum, shstrndx = struct.unpack(e + "HHH", b[0x3A:0x40])
    else:
        shoff, = struct.unpack(e + "I", b[0x20:0x24])
        shentsize, shnum, shstrndx = struct.unpack(e + "HHH", b[0x2E:0x34])
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsize
        if is64:
            name, typ, flags, addr, off, size, link = struct.unpack(e + "IIQQQQI", b[o:o + 44])
        else:
            name, typ, flags, addr, off, size, link = struct.unpack(e + "IIIIIII", b[o:o + 28])
        secs.append((typ, off, size, link))
    out = []
    for typ, off, size, link in secs:
        if typ != 2:
            continue
        stroff = secs[link][1]
        ent = 24 if is64 else 16
        for k in range(size // ent):
            o = off + k * ent
            if is64:
                name, info, other, shndx = struct.unpack(e + "IBBH", b[o:o + 8])
            else:
                name, value, sz, info, other, shndx = struct.unpack(e + "IIIBBH", b[o:o + 16])
            bind = info >> 4
            if bind in (1, 2) and shndx not in (0, 0xFFF1, 0xFFF2) and shndx < len(secs):
                s = b[stroff + name:]
                out.append(s[:s.index(b"\0")].decode("latin-1"))
    return out


def unit_objects():
    """[(unit source path, object path)] for every MWCC compile of the GameCube build."""
    cmds = subprocess.run(["ninja", "-t", "commands", "all_source"], capture_output=True, text=True).stdout
    out = []
    for c in cmds.splitlines():
        m = re.search(r" -c (\S+) -o (\S+)", c)
        if "mwcceppc" in c and m:
            src = m.group(1).replace("\\", "/")
            if src.startswith("src/"):
                obj = m.group(2).replace("\\", "/")
                if Path(obj).is_dir() or not obj.endswith(".o"):
                    obj = obj.rstrip("/") + "/" + Path(src).with_suffix(".o").name
                out.append((src[4:], obj))
    return out


def report():
    own = owners()
    defs = collections.defaultdict(list)
    missing = 0
    for unit, obj in unit_objects():
        if not Path(obj).exists():
            missing += 1
            continue
        for name in elf_defined_globals(obj):
            defs[name].append(unit)
    multi = {n: u for n, u in defs.items() if len(u) > 1}
    no_owner_copy = [n for n, u in multi.items() if own.get(n) not in u]
    print(f"{len(defs)} global symbols defined in compiled units ({missing} objects not built)")
    print(f"{len(multi)} defined in more than one unit; {len(multi) - len(no_owner_copy)} of them have a copy in the "
          f"owning unit, {len(no_owner_copy)} don't (the PC link then takes the first weak copy)")
    per_unit = collections.Counter(u for units in multi.values() for u in units)
    for u, c in per_unit.most_common(12):
        print(f"  {c:5}  {u}")


def weaken(obj, unit):
    own = owners()
    out = [n for n in elf_defined_globals(obj) if n in own and own[n] != unit]
    sys.stdout.write("".join(n + "\n" for n in out))


if __name__ == "__main__":
    if sys.argv[1:2] == ["report"]:
        report()
    elif sys.argv[1:2] == ["weaken"] and len(sys.argv) == 4:
        weaken(sys.argv[2], sys.argv[3])
    else:
        print(__doc__)
        sys.exit(2)
