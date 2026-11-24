"""Replace MWCC's `__declspec(section "...")` with the SECTION("...") macro (OrrePort stage 2).

SECTION is defined in include/dolphin/types.h: `__declspec(section name)` for the GameCube build (so its preprocessed
source is identical) and nothing under TARGET_PC. Files that use SECTION without including dolphin/types.h are
listed so the include can be added. Idempotent.

Usage: python -I tools/local/port/section_macro.py
"""
import glob
import re
from pathlib import Path

CR, LF = chr(13), chr(10)
p = Path("include/dolphin/types.h")
s = p.read_bytes().decode("utf-8")
crlf = (CR + LF) in s
s = s.replace(CR + LF, LF)
if "define SECTION(" not in s:
    a = "typedef s32 OSHeapHandle;\n#endif\n#else /* GameCube build: unchanged */"
    assert a in s
    s = s.replace(a, "typedef s32 OSHeapHandle;\n#endif\n/* MWCC places data with __declspec(section ...); on the PC the "
                     "section doesn't matter */\n#ifndef SECTION\n#define SECTION(name)\n#endif\n#else /* GameCube build: unchanged */", 1)
    b = "/* Size type for memory operations */"
    assert b in s
    s = s.replace(b, "/* Place an object in a named section (the PC build defines this as nothing) */\n"
                     "#define SECTION(name) __declspec(section name)\n\n" + b, 1)
    p.write_bytes((s.replace(LF, CR + LF) if crlf else s).encode("utf-8"))

pat = re.compile(r'__declspec\(section ("[^"]*")\)')
n = files = 0
missing = []
for f in glob.glob("src/game/**/*.c", recursive=True) + glob.glob("src/hsd/**/*.c", recursive=True) + \
        glob.glob("src/musyx/**/*.c", recursive=True) + glob.glob("include/**/*.h", recursive=True):
    if Path(f).as_posix() == "include/dolphin/types.h":
        continue
    b = Path(f).read_bytes().decode("utf-8", errors="surrogateescape")
    c = len(pat.findall(b))
    if not c:
        continue
    b = pat.sub(r"SECTION(\1)", b)
    Path(f).write_bytes(b.encode("utf-8", errors="surrogateescape"))
    n += c
    files += 1
    if "types.h" not in b and "#include" not in b:
        missing.append(f)
print(f"{n} uses in {files} files; files with no #include at all: {missing}")
