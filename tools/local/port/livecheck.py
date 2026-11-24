"""Is a C line really the scored copy of its function? (proof check for byte-identical folds; leftover-calls.md)

A fold that leaves every score unchanged proves the new argument only if the edited line is compiled into the unit
that OWNS and scores the function. Multi-unit files (fight_range_80211A00.c, gs_title.c, ...) are compiled by many
units, and candidate units compile copies that are never scored, so "byte-identical" there proves nothing.

For each `file:line`, this preprocesses every unit that includes the file (gcc -E with the unit's own defines, as the
GameCube build sees it), finds the function around the line and its owning unit in build/GC6E01/report.json, and
prints PROVEN (the owner compiles this line), COPY (compiled, but only by units that don't own the function) or DEAD
(no unit compiles it). Read-only. Needs gcc on PATH (MSYS2 mingw64).

Usage (repo root): python -I tools/local/port/livecheck.py FILE:LINE [FILE:LINE ...]   (or lines "FILE:LINE..." on stdin)
"""
import collections
import concurrent.futures as cf
import json
import os
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import callsite  # noqa: E402


def compiled_lines(files):
    cmds = subprocess.run(["ninja", "-t", "commands", "all_source"], capture_output=True, text=True).stdout.splitlines()
    units = []
    for c in cmds:
        m = re.search(r" -c (\S+) -o \S+", c)
        if "mwcceppc" in c and m and m.group(1).replace("\\", "/").startswith("src/"):
            units.append((m.group(1).replace("\\", "/"), re.findall(r"(?<=\s)-D(\S+)", c)))

    def run(u):
        src, defs = u
        text = Path(src).read_text(encoding="utf-8", errors="replace")
        if src not in files and not any(os.path.basename(f) in text for f in files):
            return set()
        r = subprocess.run(["gcc", "-E", "-x", "c", "-D__MWERKS__=0x4199", "-Iinclude", "-Ibuild/GC6E01/include", "-I."]
                           + [f"-D{d}" for d in defs] + [src], capture_output=True, text=True, errors="replace")
        out, cur, n = set(), None, 0
        for line in r.stdout.splitlines():
            m = re.match(r'^# (\d+) "([^"]+)"', line)
            if m:
                n, cur = int(m.group(1)), m.group(2).replace("\\\\", "/").replace("\\", "/").lstrip("./")
                continue
            if cur in files and line.strip():
                out.add((cur, n, src))
            n += 1
        return out

    by = collections.defaultdict(set)
    with cf.ThreadPoolExecutor(16) as ex:
        for s in ex.map(run, units):
            for f, n, src in s:
                by[(f, n)].add(src)
    return by


def owner_unit(report, name):
    for u in report["units"]:
        for f in u.get("functions", []):
            if f["name"] == name:
                return u["name"], f.get("fuzzy_match_percent")
    return None, None


def main():
    locs = [a for a in sys.argv[1:]] or [m.group(0) for l in sys.stdin for m in [re.search(r"\S+\.c:\d+", l)] if m]
    sites = [(l.rsplit(":", 1)[0], int(l.rsplit(":", 1)[1])) for l in locs]
    by = compiled_lines({f for f, _ in sites})
    report = json.load(open("build/GC6E01/report.json", encoding="utf-8"))
    for f, n in sites:
        lines = Path(f).read_text(encoding="utf-8", errors="replace").splitlines(keepends=True)
        fn, _ = callsite.enclosing(lines, n)
        unit, pct = owner_unit(report, fn) if fn else (None, None)
        src = "src/" + unit.split("/", 1)[1] + ".c" if unit else None
        units = by.get((f, n), set())
        state = "DEAD" if not units else "PROVEN" if src in units else "COPY"
        print(f"{state:6} {f}:{n}  {fn} -> owner {unit} ({pct})" + ("" if state != "COPY" else
              f"; compiled only by {', '.join(sorted(units)[:3])}{' ...' if len(units) > 3 else ''}"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
