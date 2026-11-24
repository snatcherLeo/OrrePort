"""Check port/src/pc_rel.c's OSLink against the decompiled REL modules (docs/port/rel-loading.md).

Runs pc_rel_test over the given .rel files, then, per module, compares every store its _prolog made (as run by the
PC's interpreter on the relocated PowerPC code) with the assignments in the decompiled _prolog in src/rel/<module>/
(`lbl_8047916C = lbl_131_data_0;`, `lbl_80479168 = &lbl_131_data_1A4;`), and checks that the _epilog clears every
variable the _prolog set.

Usage (pc-platform repo root):
  python -I port/tools/rel_check.py [--bin build-pc] <dir with the .rel files>
Exit code 0 = every module agrees.
"""
import argparse
import glob
import os
import re
import struct
import subprocess
import sys


def sections(path):
    r = open(path, "rb").read()
    u = lambda o: struct.unpack(">I", r[o:o + 4])[0]
    n, so = u(0x0C), u(0x10)
    return [(u(so + 8 * i) & ~1, u(so + 8 * i + 4), u(so + 8 * i) & 1) for i in range(n)]


def expected_stores(src):
    """{variable address: (kind, offset)} from the C _prolog body"""
    text = open(src, encoding="utf-8").read()
    m = re.search(r"void _prolog\s*\(void\)\s*\{(.*?)\n\}", text, re.S)
    out = {}
    for var, kind, off in re.findall(r"lbl_([0-9A-F]{8})\s*=\s*&?\s*lbl_\d+_(data|rodata|bss)_([0-9A-F]+)\s*;", m.group(1) if m else ""):
        out[int(var, 16)] = (kind, int(off, 16))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--bin", default="build-pc")
    args = ap.parse_args()
    rels = sorted(glob.glob(os.path.join(args.dir, "*.rel")))
    exe = os.path.join(args.bin, "pc_rel_test.exe" if os.name == "nt" else "pc_rel_test")
    run = subprocess.run([exe] + rels, capture_output=True, text=True)
    if run.returncode != 0:
        print(run.stdout[-2000:], run.stderr[-2000:])
        return 1
    actual, current = {}, None
    for line in run.stdout.splitlines():
        if line.startswith("module "):
            current = line.split(" ", 1)[1]
            actual[current] = {"prolog": {}, "epilog": {}}
        m = re.match(r"(prolog|epilog) store 0x([0-9A-F]{8}) <- (module\+0x([0-9A-F]+)|0x([0-9A-F]{8}))", line)
        if m and current:
            value = ("module", int(m.group(4), 16)) if m.group(4) else ("abs", int(m.group(5), 16))
            actual[current][m.group(1)][int(m.group(2), 16)] = value
    bad = 0
    checked = 0
    for path, stores in actual.items():
        name = os.path.splitext(os.path.basename(path))[0]
        srcs = glob.glob(os.path.join("src", "rel", name, "*.c"))
        secs = sections(path)
        exp = {}
        for s in srcs:
            exp.update(expected_stores(s))
        problems = []
        for var, (kind, off) in exp.items():
            got = stores["prolog"].get(var)
            if got is None or got[0] != "module":
                problems.append(f"0x{var:08X}: expected {kind}+0x{off:X}, got {got}")
                continue
            # which non-code section holds the stored address, and where in it
            hit = [(i, got[1] - o) for i, (o, size, code) in enumerate(secs) if o and not code and o <= got[1] < o + size]
            if not hit or hit[0][1] != off:
                problems.append(f"0x{var:08X}: expected {kind}+0x{off:X}, got module+0x{got[1]:X} {hit}")
        extra = set(stores["prolog"]) - set(exp)
        if extra:
            problems.append("prolog stores not in the C: " + ", ".join(f"0x{v:08X}" for v in sorted(extra)))
        not_cleared = [v for v in stores["prolog"] if stores["epilog"].get(v) != ("abs", 0)]
        if not_cleared:
            problems.append("not cleared by the epilog: " + ", ".join(f"0x{v:08X}" for v in not_cleared))
        checked += len(exp)
        status = "OK" if not problems and exp else ("NO C PROLOG FOUND" if not exp else "DIFFERENT")
        bad += status != "OK"
        print(f"{name:20s} {len(stores['prolog']):3d} prolog stores, {len(exp):3d} in the C: {status}")
        for p in problems[:5]:
            print("    " + p)
    print(f"{len(actual)} modules, {checked} stores compared: {'ALL AGREE' if bad == 0 else f'{bad} modules differ'}")
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
