"""Describe a Dolphin REL module unpacked by fsys_extract.py (read-only; prints layout only).
Usage: python -I rel_info.py <module file> [--relocs]
Prints the header, sections, imports, and the main-executable symbols the module's code refers to
(names from config/GC6E01/symbols.txt), so you can see what a module's prolog registers."""
import collections, re, struct, sys

d = open(sys.argv[1], "rb").read()
names = "id prev next numSections sectionInfoOffset nameOffset nameSize version bssSize relOffset impOffset impSize".split()
h = dict(zip(names, struct.unpack(">12I", d[:0x30])))
prolog_s, epilog_s, unres_s, bss_s = d[0x30:0x34]
prolog, epilog, unres = struct.unpack(">III", d[0x34:0x40])
print({k: hex(v) for k, v in h.items()})
print(f"prolog sec{prolog_s}+{prolog:#x} epilog sec{epilog_s}+{epilog:#x} unresolved sec{unres_s}+{unres:#x}")
for s in range(h["numSections"]):
    off, size = struct.unpack(">II", d[h["sectionInfoOffset"] + 8 * s:h["sectionInfoOffset"] + 8 * s + 8])
    if size: print(f"  section {s}: offset {off & ~1:#x} size {size:#x}{' (code)' if off & 1 else ''}")
syms = {}
for l in open("config/GC6E01/symbols.txt", encoding="utf-8"):
    m = re.match(r"(\S+) = \.\w+:0x([0-9A-F]+);", l)
    if m: syms[int(m.group(2), 16)] = m.group(1)
imp = d[h["impOffset"]:h["impOffset"] + h["impSize"]]
refs = collections.Counter()
for k in range(0, len(imp), 8):
    mid, off = struct.unpack(">II", imp[k:k + 8])
    p = off; sec = None; pos = 0; n = 0
    while True:
        o, t, s, a = struct.unpack(">HBBI", d[p:p + 8]); p += 8
        if t == 203: break
        if t == 202: sec = s; pos = 0; continue
        pos += o
        if t == 201: continue
        n += 1
        if mid == 0: refs[syms.get(a, hex(a))] += 1
        if "--relocs" in sys.argv: print(f"    sec{sec}+{pos:#x} type {t} -> {'main' if mid == 0 else 'module'} {syms.get(a, hex(a)) if mid == 0 else f'sec{s}+{a:#x}'}")
    print(f"  imports module {mid:#x}: {n} relocations")
print("main-executable symbols referenced:", ", ".join(f"{k} x{v}" for k, v in refs.most_common(40)))
