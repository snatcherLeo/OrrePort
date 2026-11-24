"""Verify pcdata.py's output byte for byte (OrrePort; the project notes section 6).

For every symbol pcdata.py converted (typed values, strings, zero data), this computes the bytes the PC must hold
straight from the original big-endian bytes (a per-element byte swap in Python, independent of the C literals the
generator wrote), writes a C test program into build-pc/generated/ that compares them with what the compiler made of
the generated definitions, builds it with the PC compiler and runs it. Pointer slots are skipped here (their values
are PC addresses, checked by pc_data_relocate()'s own tests); unsure symbols must be the untouched original bytes and
are compared as such.
Usage (repo root, MSYS2 mingw64 on PATH): python -I tools/local/port/pcdata_check.py
"""
import re
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pcdata  # noqa: E402

GEN = Path("build-pc/generated")


def main():
    src = (GEN / "pc_data_main.c").read_text(encoding="utf-8")
    objs = {}
    for o in sorted(pcdata.OBJ_ROOT.rglob("*.o")):
        for name, rec in pcdata.read_elf(o).items():
            objs.setdefault(name, rec)
    pat = re.compile(r"^(u8|s8|char|unsigned char|u16|s16|short|unsigned short|u32|s32|int|unsigned int|BOOL|unsigned|"
                     r"long|unsigned long|u64|s64|f32|float|f64|double|void\*) (\w+)(\[(\d+)\])? .*?(=|;)(.*)$", re.M)
    checks, skipped = [], 0
    reloc_names = set(re.findall(r"\{\(char\*\)(\w+) \+ (\d+),", src))
    reloc_slots = {}
    for n, off in re.findall(r"\{\(char\*\)(\w+) \+ (\d+),", src):
        reloc_slots.setdefault(n, set()).add(int(off))
    for m in pat.finditer(src):
        ty, name = m.group(1), m.group(2)
        if ty == "void*" or name not in objs:
            skipped += 1
            continue
        _, data, rel, nobits = objs[name]
        raw = "unsure: raw big-endian" in m.group(0)
        if nobits:
            expected = bytes(len(data))
        elif raw or ty in ("u8", "s8", "char", "unsigned char"):
            expected = data
        else:
            size, fmt, _ = pcdata.SCALARS[ty]
            n = len(data) // size
            vals = struct.unpack(">" + fmt * n, data)
            expected = struct.pack("<" + fmt * n, *vals)
        mask = bytearray(b"\x01" * len(expected))
        for off in reloc_slots.get(name, ()):  # pointer slots: filled at startup, not checked here
            mask[off:off + 4] = b"\x00\x00\x00\x00"
        checks.append((name, expected, bytes(mask)))
    lines = [f"/* {pcdata.MARKER}: test data derived from the original program; never commit. */",
             '#include <stdio.h>', '#include <string.h>', '#include <stdint.h>']
    lines += [f"extern unsigned char {n}[];" for n, _, _ in checks]
    # placeholders for the pointer targets that live in the game's C code (not linked into this test)
    lines += [f"char {n}[4];" for n in re.findall(r"^extern char (\w+)\[\];$", src, re.M)]
    lines += [f"void {n}(void) {{}}" for n in re.findall(r"^extern void (\w+)\(void\);", src, re.M)]
    lines += ["static int check(const char* name, const unsigned char* got, const unsigned char* want, const unsigned char* mask, unsigned n)",
              "{ for (unsigned i = 0; i < n; i++) if (mask[i] && got[i] != want[i]) { printf(\"MISMATCH %s +0x%X: got %02X want %02X\\n\", name, i, got[i], want[i]); return 1; } return 0; }",
              "int main(void) { int bad = 0;"]
    for i, (n, want, mask) in enumerate(checks):
        lines.append(f'  {{ static const unsigned char w[] = {{{", ".join(str(x) for x in want) or "0"}}};'
                     f' static const unsigned char k[] = {{{", ".join(str(x) for x in mask) or "0"}}};'
                     f' bad += check("{n}", {n}, w, k, {len(want)}); }}')
    # pointer slots: after pc_data_relocate() each must hold its target's address (big- or little-endian)
    lines += ["  { struct r { void* slot; void* target; int big; }; extern struct r pc_data_relocs[];",
              "    extern const unsigned pc_data_reloc_count; extern void pc_data_relocate(void); int badr = 0;",
              "    pc_data_relocate();",
              "    for (unsigned i = 0; i < pc_data_reloc_count; i++) {",
              "      unsigned char* s = pc_data_relocs[i].slot; uint32_t want, got; if (!s) continue;",
              "      want = (uint32_t)(uintptr_t)pc_data_relocs[i].target;",
              "      if (pc_data_relocs[i].big) got = (uint32_t)s[0] << 24 | (uint32_t)s[1] << 16 | (uint32_t)s[2] << 8 | s[3];",
              "      else memcpy(&got, s, 4);",
              "      if (got != want) { printf(\"RELOC MISMATCH %u\\n\", i); badr++; } }",
              "    printf(\"%d of %u pointer slots wrong\\n\", badr, pc_data_reloc_count); bad += badr; }"]
    lines += [f'  printf("%d of {len(checks)} symbols differ\\n", bad); return bad != 0; }}']
    (GEN / "pc_data_check.c").write_text("\n".join(lines) + "\n", encoding="utf-8")
    exe = GEN / "pc_data_check.exe"
    # linked like the port: low image base, no ASLR, so every address is below 4 GB (port-gaps.md 6.3)
    r = subprocess.run(["gcc", "-O0", "-o", str(exe), str(GEN / "pc_data_check.c"), str(GEN / "pc_data_main.c"),
                        "-Wl,--image-base,0x10000000", "-Wl,--disable-dynamicbase"], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr[-3000:])
        return 2
    r = subprocess.run([str(exe)], capture_output=True, text=True)
    print(r.stdout.strip()[-3000:])
    print(f"checked {len(checks)} symbols (pointer-typed symbols and pointer slots skipped: {skipped})")
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
