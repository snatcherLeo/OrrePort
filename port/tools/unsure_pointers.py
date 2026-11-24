"""List pcdata.py's "unsure" data symbols that the C code declares as native pointers or structs (first-link.md).

pcdata.py emits a symbol it can't type safely as raw big-endian bytes, and fills its pointer words big-endian at
startup. If the C code declares that symbol as a native pointer (`struct MessageSystem* _pMw`), the game reads
a byte-swapped value: the first crash of the first PC run (GSmsgInit). This lists every such symbol, so they can get
type info in pcdata_types.txt. Reads build-pc/generated (pcdata.py's output) and src/; writes
build-pc/firstlink/unsure_native_pointers.txt.

Usage (pc-platform repo root): python -I port/tools/unsure_pointers.py
"""
import collections
import glob
import re
from pathlib import Path

gen = Path("build-pc/generated/pc_data_main.c").read_text(encoding="utf-8")
unsure = {l.split("\t")[0] for l in Path("build-pc/generated/unsure_data.txt").read_text(encoding="utf-8").splitlines()
          if l.startswith("lbl_")}
reloc = collections.Counter(re.findall(r"\{\(char\*\)(lbl_[0-9A-F]{8}) \+ \d+,", gen))
decl = re.compile(r"^\s*(?:extern\s+)?(?:const\s+)?(?:struct\s+)?[A-Za-z_]\w*\s*\*+\s*(?:const\s+)?(lbl_[0-9A-F]{8})\s*;",
                  re.M)
where = collections.defaultdict(set)
for f in glob.glob("src/game/**/*.c", recursive=True) + glob.glob("src/hsd/**/*.c", recursive=True):
    for m in decl.finditer(Path(f).read_text(encoding="utf-8", errors="replace")):
        where[m.group(1)].add(f.replace("\\", "/"))
# also: unsure symbols whose conflicting declarations include a struct type (HSD class records such as
# HSD_MObjInfo: function-pointer tables the code calls through natively)
reasons = {l.split("	")[0]: l.split("	")[2] for l in
           Path("build-pc/generated/unsure_data.txt").read_text(encoding="utf-8").splitlines()
           if l.startswith("lbl_") and l.count("	") >= 2}
for n, r in reasons.items():
    m = re.match(r"conflicting declarations: ([^;]+)", r)
    # a struct type, a pointer to one, or an array of them (`FSYSDispatchEntry[]`: the fsys state machine, run 29)
    if m and reloc[n] and any(re.fullmatch(r"(struct\s+)?[A-Z]\w+\s*(\*|\[\w*\])?", t.strip())
                              for t in m.group(1).split(",")):
        where[n].add("declared as " + m.group(1).strip())
hits = sorted(n for n in unsure if reloc[n] and n in where)
out = Path("build-pc/firstlink/unsure_native_pointers.txt")
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text("".join(f"{n}\t{reloc[n]} pointer word(s)\t{', '.join(sorted(where[n]))}\n" for n in hits),
               encoding="utf-8")
print(f"{len(unsure)} unsure symbols; {sum(1 for n in unsure if reloc[n])} hold relocated pointer words; {len(hits)} of "
      f"those are declared as a native pointer in C -> {out}")
