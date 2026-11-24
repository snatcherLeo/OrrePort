"""A merge with upstream that sees through upstream's renames (2026-10-18; after apply_renames.py).

    python -I tools/local/port/rename_merge.py <upstream rev> [--write]     (run inside the checkout to merge into)

`git merge-tree` compares both sides against the merge base, which still has the old names: a line upstream renamed and
a line we wrapped in `#ifndef TARGET_PC` overlap, so it conflicts even after our side uses the new names. This tool
redoes each conflicted file's three-way merge with the **base renamed the same way** (apply_renames.rename_map, by
address), so each side's diff holds only its real edits; Git's own `git merge-file` does the merging.

Steps: `git merge-tree --write-tree HEAD <upstream>` (the tree with every clean merge, and stages 1-3 of each
conflicted file); with --write, read that tree into the index and work tree (`git read-tree -m -u`), then overwrite
each conflicted file with the rename-aware merge. Files that still conflict keep Git's markers and are listed; nothing
is committed (the caller commits with two parents after resolving and gating).
"""
import os, subprocess, sys, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import apply_renames  # noqa: E402


def git(*args, binary=False, check=True):
    r = subprocess.run(["git", *args], capture_output=True, check=False)
    if check and r.returncode not in (0, 1):
        raise RuntimeError(f"git {' '.join(args)}: {r.stderr.decode(errors='replace')}")
    return r.stdout if binary else r.stdout.decode(errors="replace")


def main():
    up = sys.argv[1]
    write = "--write" in sys.argv
    base = git("merge-base", "HEAD", up).strip()
    out = git("merge-tree", "--write-tree", "HEAD", up)
    lines = out.split("\n")
    tree = lines[0].strip()
    stages = {}
    for line in lines[1:]:
        if not line.strip():
            break
        meta, path = line.split("\t", 1)
        mode, obj, stage = meta.split()
        stages.setdefault(path, {})[int(stage)] = obj
    ren, _ = apply_renames.rename_map(base, up)
    import re
    names = sorted(ren, key=len, reverse=True)
    pat = re.compile(rb"(?<![A-Za-z0-9_])(" + b"|".join(re.escape(n.encode()) for n in names) + rb")(?![A-Za-z0-9_])")
    table = {k.encode(): v.encode() for k, v in ren.items()}
    print(f"merge tree {tree}; {len(stages)} conflicted files; renaming the base with {len(ren)} renames")
    if write:
        git("read-tree", "-m", "-u", "HEAD", tree)
    still, clean = [], 0
    tmp = tempfile.mkdtemp(prefix="rename_merge_")
    for path, st in sorted(stages.items()):
        blob = lambda k: git("cat-file", "blob", st[k], binary=True) if k in st else b""
        b, o, t = blob(1), blob(2), blob(3)
        b2 = pat.sub(lambda m: table[m.group(1)], b)
        fb, fo, ft = (os.path.join(tmp, n) for n in ("base", "ours", "theirs"))
        for f, data in ((fb, b2), (fo, o), (ft, t)):
            open(f, "wb").write(data)
        r = subprocess.run(["git", "merge-file", "-p", "-L", "ours (renamed)", "-L", "base (renamed)",
                            "-L", "upstream", fo, fb, ft], capture_output=True)
        n = r.returncode
        if n < 0 or n > 127:
            raise RuntimeError(f"merge-file failed on {path}")
        if n:
            still.append((path, n))
        else:
            clean += 1
        if write:
            os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
            open(path, "wb").write(r.stdout)
    print(f"now clean: {clean}; still conflicting: {len(still)} files, "
          f"{sum(n for _, n in still)} hunks")
    for p, n in still:
        print(f"  {n}\t{p}")


if __name__ == "__main__":
    main()
