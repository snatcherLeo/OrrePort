"""Check the data pointers pc_rel.c's OSLink writes into REL module data against the decompiled C (
docs/port/rel-loading.md).

The decompiled modules in src/rel/<module>/ initialise their tables in C, e.g.
`FieldTable lbl_131_data_1A8 = {&lbl_131_data_1A4, lbl_131_data_0};`: after linking, the module's bytes at that
label must hold the big-endian addresses of the labels it names. This links each module with pc_rel_test (dumping
the result), then checks every initializer that is made of module labels only.

Usage (pc-platform repo root):
  python -I port/tools/rel_data_check.py [--bin build-pc] <dir with the .rel files>
Exit code 0 = every checked pointer agrees.
"""
import argparse
import glob
import os
import re
import struct
import subprocess
import sys
import tempfile

INIT = re.compile(r"^[A-Za-z_][\w ]*?\s\*?\s*lbl_(\d+)_data_([0-9A-F]+)(?:\[[^\]]*\])*\s*=\s*\{([^;]*)\};", re.M)
ITEM = re.compile(r"^&?\s*lbl_(\d+)_data_([0-9A-F]+)$")


def data_section(module):
    """offset of the module's .data section: the largest non-code section"""
    u = lambda o: struct.unpack(">I", module[o:o + 4])[0]
    n, so = u(0x0C), u(0x10)
    secs = [(u(so + 8 * i) & ~1, u(so + 8 * i + 4), u(so + 8 * i) & 1) for i in range(n)]
    return max((s for s in secs if s[0] and not s[2]), key=lambda s: s[1])[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--bin", default="build-pc")
    args = ap.parse_args()
    rels = sorted(glob.glob(os.path.join(args.dir, "*.rel")))
    exe = os.path.join(args.bin, "pc_rel_test.exe" if os.name == "nt" else "pc_rel_test")
    with tempfile.TemporaryDirectory() as tmp:
        env = dict(os.environ, PC_REL_DUMP_DIR=tmp)
        run = subprocess.run([exe] + rels, capture_output=True, text=True, env=env)
        if run.returncode != 0:
            print(run.stdout[-1500:], run.stderr[-1500:])
            return 1
        total = bad = 0
        for rel in rels:
            name = os.path.splitext(os.path.basename(rel))[0]
            dump = open(os.path.join(tmp, os.path.basename(rel) + ".linked"), "rb").read()
            base, module = struct.unpack("<I", dump[:4])[0], dump[4:]
            data = data_section(module)
            checked = wrong = 0
            for src in glob.glob(os.path.join("src", "rel", name, "*.c")):
                for mid, off, body in INIT.findall(open(src, encoding="utf-8").read()):
                    items = [x.strip() for x in body.replace("\n", " ").split(",") if x.strip()]
                    parsed = [ITEM.match(x) for x in items]
                    if not items or not all(parsed):
                        continue  # only initializers made of module labels (pointers) are checked
                    at = data + int(off, 16)
                    for k, m in enumerate(parsed):
                        want = base + data + int(m.group(2), 16)
                        got = struct.unpack(">I", module[at + 4 * k:at + 4 * k + 4])[0]
                        checked += 1
                        if got != want:
                            wrong += 1
                            if wrong <= 3:
                                print(f"  {name}: data+0x{int(off, 16) + 4 * k:X} holds 0x{got:08X}, "
                                      f"C says lbl_{mid}_data_{m.group(2)} = 0x{want:08X}")
            total += checked
            bad += wrong
            print(f"{name:20s} {checked:4d} data pointers checked: {'OK' if wrong == 0 else f'{wrong} WRONG'}")
        print(f"{len(rels)} modules, {total} data pointers: {'ALL AGREE' if bad == 0 else f'{bad} differ'}")
        return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
