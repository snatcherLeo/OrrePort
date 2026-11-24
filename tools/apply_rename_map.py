"""Bring code written against older names up to date with the rename map (2026-11-24; promised in CONTRIBUTING.md).

The project gives functions and data labels readable names in batches (docs/NAMING_SCHEME.md). Each rename is a row in
docs/function_renames.csv or docs/data_renames.csv: address, old name, new name and the evidence. Code or notes written
against an older name (a fork, a patch, a mod, a contributor's branch) can be updated with this script:

    python -I tools/apply_rename_map.py <file or folder> ...            list what would change (nothing is written)
    python -I tools/apply_rename_map.py <file or folder> ... --apply    write the changes

How it works:
  - every old name maps to its FINAL new name (a name renamed twice goes straight to the last one), so one run is
    enough and a second run changes nothing;
  - whole words only (fn_80006250 never matches inside fn_800062500 or my_fn_80006250);
  - in C / C++ / assembly sources, names inside string and character literals are left alone (a string is text the
    program shows or looks up, never a symbol); comments are updated, so notes keep pointing at the right function;
  - other text files (Markdown, plain text, CSV) are updated everywhere;
  - files the map doesn't touch are never rewritten.
The address column is the lasting key: when in doubt about a name, look it up by address in config/GC6E01/symbols.txt.

Run it on YOUR code (a fork, a patch, a mod), not on this repository's own sources: those already use the new names,
and a few places keep an old name on purpose (alias headers such as port/src/musyx_pc_alias.h map an address name to
its readable one for the PC link; history notes quote the name a function had when they were written). A dry run over
src/ lists those (2026-11-24: 442 uses in 107 files), which is expected.

Platforms: plain Python 3, any OS.
"""
import argparse
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAPS = [os.path.join(ROOT, "docs", "function_renames.csv"), os.path.join(ROOT, "docs", "data_renames.csv")]
CODE = (".c", ".h", ".cpp", ".hpp", ".cc", ".inl", ".s", ".S", ".inc")
TEXT = (".md", ".txt", ".csv", ".tsv", ".py")


def load_map():
    step = {}
    for path in MAPS:
        with open(path, encoding="utf-8", newline="") as f:
            for row in csv.DictReader(f):
                old, new = row["old_name"].strip(), row["new_name"].strip()
                if old and new and old != new:
                    step[old] = new
    final = {}
    for old in step:  # follow each chain to its end (a -> b -> c gives a -> c and b -> c)
        seen, name = {old}, step[old]
        while name in step:
            if name in seen:
                raise SystemExit(f"the rename map has a cycle through {name}")
            seen.add(name)
            name = step[name]
        final[old] = name
    return final


def rename_code(src, rx, final):
    """names outside string and character literals (comments included)"""
    out, i, n, count = [], 0, len(src), 0
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] in "/*":  # a comment: names are updated
            end = src.find("\n", i) if src[i + 1] == "/" else src.find("*/", i + 2) + 2
            if end <= 1:
                end = n
            part, k = rx.subn(lambda m: final[m.group(0)], src[i:end])
            out.append(part)
            count += k
            i = end
            continue
        if c in "\"'":  # a literal: left as it is
            j = i + 1
            while j < n and src[j] != c and src[j] != "\n":
                j += 2 if src[j] == "\\" else 1
            out.append(src[i:j + 1])
            i = j + 1
            continue
        j = i
        while j < n and src[j] not in "\"'/":
            j += 1
        if j == i:  # a lone "/" (division)
            j = i + 1
        part, k = rx.subn(lambda m: final[m.group(0)], src[i:j])
        out.append(part)
        count += k
        i = j
    return "".join(out), count


def files_under(paths):
    for p in paths:
        if os.path.isfile(p):
            yield p
            continue
        for d, dirs, names in os.walk(p):
            dirs[:] = [x for x in dirs if not x.startswith(".") and x not in ("build", "build-pc", "orig", "deps")]
            for name in names:
                if name.endswith(CODE + TEXT):
                    yield os.path.join(d, name)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("paths", nargs="+")
    ap.add_argument("--apply", action="store_true", help="write the changes (default: only list them)")
    a = ap.parse_args()
    final = load_map()
    # longest names first, so a name that contains another never half-matches; \b keeps it to whole words
    rx = re.compile(r"\b(?:" + "|".join(map(re.escape, sorted(final, key=len, reverse=True))) + r")\b")
    changed_files = total = 0
    for path in files_under(a.paths):
        try:
            with open(path, encoding="utf-8", newline="") as f:
                text = f.read()
        except UnicodeDecodeError:
            continue
        if path.endswith(CODE):
            new, count = rename_code(text, rx, final)
        else:
            new, count = rx.subn(lambda m: final[m.group(0)], text)
        if not count:
            continue
        changed_files += 1
        total += count
        names = sorted({m.group(0) for m in rx.finditer(text)})
        shown = ", ".join(f"{o} -> {final[o]}" for o in names[:4]) + (" ..." if len(names) > 4 else "")
        print(f"{path}: {count} name(s): {shown}")
        if a.apply:
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.write(new)
    verb = "changed" if a.apply else "would change (run again with --apply to write)"
    print(f"{total} name(s) in {changed_files} file(s) {verb}; the map has {len(final)} old names")
    return 0


if __name__ == "__main__":
    sys.exit(main())
