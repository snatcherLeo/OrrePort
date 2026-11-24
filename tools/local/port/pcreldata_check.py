"""Verify pcreldata.py's output by running it (OrrePort; port-gaps.md section 8).

Builds a test program from build-pc/generated/pc_rel_modules.c with placeholder definitions for the main-program
symbols it references (linked like the port: image at 0x10000000, no ASLR), then for every generated module:
  - pc_rel_link(id) returns 1, every pointer word of that module holds its target's address (big-endian), and every
    prolog store wrote its value into its slot;
  - pc_rel_unlink(id) runs every epilog store (the slots end up holding the epilog's values, normally 0).
Also checks that an unknown module id is refused (returns 0). The test program contains disc-derived data, so it is
written to build-pc/generated/ with the marker line and never committed.
Usage (repo root, MSYS2 mingw64 on PATH): python -I tools/local/port/pcreldata_check.py
"""
import re
import subprocess
import sys
from pathlib import Path

GEN = Path("build-pc/generated")
MARKER = "ORREPORT-" + "GENERATED-FROM-DISC"


def main():
    src = (GEN / "pc_rel_modules.c").read_text(encoding="utf-8")
    lines = [f"/* {MARKER}: test program for pc_rel_modules.c; never commit. */",
             "#include <stdio.h>", "#include <stdint.h>", "#include <string.h>", "typedef uint8_t u8; typedef uint32_t u32;"]
    # placeholder main-program symbols, big enough for any offset the modules use
    offs = {}
    for name, off in re.findall(r"\(\(char\*\)&(\w+) \+ (\d+)\)", src):
        offs[name] = max(offs.get(name, 0), int(off))
    for name in re.findall(r"^extern char (\w+)\[\];", src, re.M):
        lines.append(f"char {name}[{offs.get(name, 0) + 8}] __attribute__((aligned(8)));")
    for name in re.findall(r"^extern void (\w+)\(void\);", src, re.M):
        lines.append(f"void {name}(void) {{}}")
    lines += [
        "struct pc_rel_ptr { u32 module; u8* slot; void* target; };",
        "struct pc_rel_store { u32 module; int epilog; u32* slot; void* value; };",
        "extern struct pc_rel_ptr pc_rel_ptrs[]; extern struct pc_rel_store pc_rel_stores[]; extern const u32 pc_rel_ids[];",
        "extern const unsigned pc_rel_store_count, pc_rel_ptr_count, pc_rel_id_count;",
        "int pc_rel_link(u32 module); int pc_rel_unlink(u32 module);",
        "static u32 be(const u8* s) { return (u32)s[0] << 24 | (u32)s[1] << 16 | (u32)s[2] << 8 | s[3]; }",
        "int main(void) {",
        "  int bad = 0; unsigned nptr = 0, nst = 0, nun = 0;",
        "  if (pc_rel_link(0xFFFFFF)) { printf(\"unknown module accepted\\n\"); bad++; }",
        "  for (unsigned m = 0; m < pc_rel_id_count; m++) {",
        "    u32 id = pc_rel_ids[m];",
        "    if (pc_rel_link(id) != 1) { printf(\"module %u not linked\\n\", id); bad++; continue; }",
        "    for (unsigned i = 0; i < pc_rel_ptr_count; i++) if (pc_rel_ptrs[i].module == id) { nptr++;",
        "      if (be(pc_rel_ptrs[i].slot) != (u32)(uintptr_t)pc_rel_ptrs[i].target) { printf(\"ptr %u of module %u wrong\\n\", i, id); bad++; } }",
        "    for (unsigned i = 0; i < pc_rel_store_count; i++) if (pc_rel_stores[i].module == id && !pc_rel_stores[i].epilog) { nst++;",
        "      if (*pc_rel_stores[i].slot != (u32)(uintptr_t)pc_rel_stores[i].value) {",
        "        /* a later prolog store of the same module may overwrite the same slot: only the last one counts */",
        "        unsigned j, later = 0; for (j = i + 1; j < pc_rel_store_count; j++)",
        "          if (pc_rel_stores[j].module == id && !pc_rel_stores[j].epilog && pc_rel_stores[j].slot == pc_rel_stores[i].slot) later = 1;",
        "        if (!later) { printf(\"store %u of module %u wrong\\n\", i, id); bad++; } } }",
        "    pc_rel_unlink(id);",
        "    for (unsigned i = 0; i < pc_rel_store_count; i++) if (pc_rel_stores[i].module == id && pc_rel_stores[i].epilog) { nun++;",
        "      if (*pc_rel_stores[i].slot != (u32)(uintptr_t)pc_rel_stores[i].value) { printf(\"epilog %u of module %u wrong\\n\", i, id); bad++; } }",
        "  }",
        "  printf(\"%u modules, %u pointer words, %u prolog stores, %u epilog stores checked; %d wrong\\n\", pc_rel_id_count, nptr, nst, nun, bad);",
        "  return bad != 0;",
        "}"]
    (GEN / "pc_rel_check.c").write_text("\n".join(lines) + "\n", encoding="utf-8")
    exe = GEN / "pc_rel_check.exe"
    r = subprocess.run(["gcc", "-O0", "-o", str(exe), str(GEN / "pc_rel_check.c"), str(GEN / "pc_rel_modules.c"),
                        "-Wl,--image-base,0x10000000", "-Wl,--disable-dynamicbase"], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr[-3000:])
        return 2
    r = subprocess.run([str(exe)], capture_output=True, text=True)
    print(r.stdout.strip()[-3000:])
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
