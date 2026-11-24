"""Raw-offset reads that likely touch disc data (OrrePort; the project notes section 8, "largest getting-it-right item").

Game code reads data through casts like `*(u32*)(p + 4)`. On the PC, disc-loaded data (common_rel's tables, room
modules, HSD archives) stays big-endian, so such a read of disc data returns the wrong value unless it goes through a
big-endian view or accessor. This is a first, heuristic triage: for every wide raw read (u16 / s16 / u32 / s32 / f32
through a pointer cast) in src/game it reports whether the same function also mentions
  - a global that a room module's prolog publishes (build-pc/generated/pc_rel_stores.txt: common_rel's table
    pointers, etc.), or
  - a call to a `...BiosGetPtr` / `...DataBiosGetPtr` accessor (the per-table getters over those tables).
Functions with such a mention are "likely disc" (the base pointer often comes from there); others are "unknown".
Output: a per-file count and the list of likely-disc functions in build-pc/pc_rawreads.txt. Heuristic: a starting
list for strategy (a), not proof. Read-only.

Usage (repo root): python -I tools/local/port/rawreads.py
"""
import collections
import re
from pathlib import Path

RAW = re.compile(r"\*\s*\(\s*(?:volatile\s+)?(?:const\s+)?(u16|s16|u32|s32|f32|f64)\s*\*\s*\)")
FUNC = re.compile(r"^[A-Za-z_][\w\s\*]*?\b(\w+)\s*\([^;{}]*\)\s*(?:/\*[^\n]*\*/\s*)?\{", re.M)


def rel_globals():
    p = Path("build-pc/generated/pc_rel_stores.txt")
    if not p.exists():
        return set()
    return set(re.findall(r"(lbl_[0-9A-F]{8}) <- module", p.read_text(encoding="utf-8", errors="replace")))


def main():
    rel = rel_globals()
    a_files = ("gs_gfx", "gs_model", "gs_light", "gs_camera", "gs_texture", "gs_material", "gs_render_util")
    per_file = collections.Counter()
    likely = []
    total = likely_n = 0
    for p in sorted(Path("src/game").rglob("*.c")):
        if any(a in p.name for a in a_files):
            continue
        t = p.read_text(encoding="utf-8", errors="replace")
        starts = [(m.start(), m.group(1)) for m in FUNC.finditer(t)]
        for k, (s, name) in enumerate(starts):
            e = starts[k + 1][0] if k + 1 < len(starts) else len(t)
            body = t[s:e]
            n = len(RAW.findall(body))
            if not n:
                continue
            total += n
            disc = any(g in body for g in rel) or re.search(r"\w+BiosGetPtr\s*\(", body)
            if disc:
                likely_n += n
                per_file[p.as_posix()] += n
                likely.append(f"{p.as_posix()}: {name} ({n})")
    out = [f"{total} raw wide reads in the project's files; {likely_n} in functions that also touch a disc table (likely disc)", ""]
    out += [f"{c:5d}  {f}" for f, c in per_file.most_common()]
    out += ["", "functions:"] + likely
    Path("build-pc/pc_rawreads.txt").write_text("\n".join(out) + "\n", encoding="utf-8")
    print(out[0])
    print("\n".join(out[2:22]))


if __name__ == "__main__":
    main()
