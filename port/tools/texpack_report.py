"""port/tools/texpack_report.py (B, 2026-10-17, snatcherLeo's texture-pack experiment): which files of a Dolphin-format
texture pack OrrePort uses, from one run's log.

Usage: python -I port/tools/texpack_report.py <run log> <pack folder>
  run log:     <exe>/logs/orreport_*.log of a run with ORRE_TEXTURE_REPORT=1 (pc_textures.cpp)
  pack folder: <exe>/user/textures/GC6E01

Aurora logs, once per texture the game draws: "texture_replacement: applied <name>" when a pack file covers it, and
"texture_replacement: missing runtime key <key>" when none does, followed by the pack's near misses
("candidate (same dimensions) ... path=<file>"). The report lists:
  1. pack files that applied;
  2. pack files that never matched anything drawn in this run (either not reached, or a mismatch);
  3. near misses: a texture the game drew with no pack file, where a pack file has the same size but a different
     hash. Those are the interesting ones: the PC may decode or hash that texture differently from Dolphin (or the
     pack targets another region / version). Read-only.
"""
import re
import sys
from pathlib import Path


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    log = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
    pack = Path(sys.argv[2])
    files = {p.name.lower(): p for p in pack.rglob("*") if p.suffix.lower() in (".dds", ".png")}

    applied = set()
    near = []  # (missing key text, reason, pack file)
    missing = 0
    current = None
    for line in log:
        m = re.search(r"texture_replacement: applied ([^\s\]]+)", line)
        if m:
            applied.add(m.group(1).lower())
            continue
        m = re.search(r"texture_replacement: missing runtime key (.*?)\]?$", line)
        if m:
            missing += 1
            current = m.group(1).strip()
            continue
        m = re.search(r"texture_replacement: candidate \(([^)]*)\) .*path=(.*?)\]?$", line)
        if m and current is not None:
            near.append((current, m.group(1), Path(m.group(2).strip()).name))

    def stem_key(name):  # tex1_WxH[_m]_hash[_tlut]_fmt: drop the extension and "_arb" / "_mipN" suffixes
        s = re.sub(r"\.(dds|png)$", "", name.lower())
        return re.sub(r"(_arb|_mip\d+)$", "", s)

    applied_stems = {stem_key(a) for a in applied}
    used = sorted(n for n in files if stem_key(n) in applied_stems)
    unused = sorted(n for n in files if stem_key(n) not in applied_stems and "_mip" not in n)

    print(f"pack: {len(files)} file(s) in {pack}")
    print(f"run: {len(applied)} texture(s) drawn from the pack, {missing} drawn with no pack file\n")
    print(f"1. applied ({len(used)}):")
    for n in used:
        print(f"   {n}")
    print(f"\n2. never matched in this run ({len(unused)}): not reached in this run, or a hash / size mismatch")
    for n in unused[:200]:
        print(f"   {n}")
    if len(unused) > 200:
        print(f"   ... {len(unused) - 200} more")
    # A pack file that never applied, with a same-size texture the game drew instead, is the suspect case: that file
    # was probably made for this texture, and the PC computes another hash (it decodes the texture differently, or the
    # pack is for another version). Same-size pairs for files that did apply are just other textures of that size.
    unused_set = set(unused)
    suspects = [x for x in near if "dimensions" in x[1] and x[2].lower() in unused_set]
    print(f"\n3. suspects ({len(suspects)}): a pack file that never applied, and a texture of the same size and format "
          "the game drew with another hash; likely decoded or hashed differently on the PC, or a pack for another "
          "version (or simply a different texture of that size)")
    for key, reason, f in suspects[:100]:
        print(f"   game drew {key}\n      pack has {f} ({reason})")


if __name__ == "__main__":
    main()
