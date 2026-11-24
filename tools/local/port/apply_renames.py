"""Apply upstream's symbol renames to our side before a merge (2026-10-18; the project notes merge protocol).

    python -I tools/local/port/apply_renames.py <base rev> <upstream rev> [--write] [--report FILE] [paths...]

Upstream gives functions and data labels real names (`fn_800E27B0` -> `GSmemLock`) in config/GC6E01/symbols.txt. Our
side still uses the old names, including in PC-only code a plain merge doesn't see (port/, include/pc/, the PC data
generators). This tool builds the rename map by address (section + address in symbols.txt at <base> and at
<upstream>) and replaces whole identifiers in our files, so the merge that follows only has real conflicts left.

Safety: bytes in, bytes out (no text decoding, no escape handling); only whole identifiers ([A-Za-z0-9_] boundaries);
only source-like files (.c .h .cpp .hpp .py .cmake .txt CMakeLists, .s/.S are left out); never .inc files, never
config/GC6E01/ (symbols.txt and splits.txt come from upstream in the merge), never docs/ or extracted/. A rename is
skipped (and reported) when its old name is still a different address's name upstream, or its new name already
names another address at <base> (both would change meaning). Without --write it only reports.
"""
import argparse, os, re, subprocess, sys

EXTS = (".c", ".h", ".cpp", ".hpp", ".py", ".cmake")
NAMES = ("CMakeLists.txt",)
SKIP_DIRS = {".git", "build", "build-pc", "orig", "extracted", "docs", "archive", "__pycache__"}
DEFAULT_PATHS = ["src", "include", "port", "tools/local/port", "configure.py", "tests"]


def symbols(rev):
    text = subprocess.run(["git", "show", f"{rev}:config/GC6E01/symbols.txt"], capture_output=True, text=True,
                          check=True).stdout
    by_addr, by_name = {}, {}
    for line in text.splitlines():
        m = re.match(r"(\S+) = (\S+):(0x[0-9A-Fa-f]+);", line)
        if m:
            key = (m.group(2), m.group(3).lower())
            by_addr[key] = m.group(1)
            by_name.setdefault(m.group(1), set()).add(key)
    return by_addr, by_name


def rename_map(base, new):
    b, b_names = symbols(base)
    u, u_names = symbols(new)
    ident = re.compile(r"[A-Za-z_][A-Za-z0-9_]*$")
    cand = {}
    for key, old in b.items():
        nw = u.get(key)
        if nw and nw != old:
            cand.setdefault(old, set()).add(nw)
    ren, skipped = {}, []
    for old, nws in cand.items():
        if len(nws) != 1:
            skipped.append((old, sorted(nws), "old name maps to two new names"))
            continue
        nw = next(iter(nws))
        if not ident.match(old) or not ident.match(nw):
            skipped.append((old, nw, "not a C identifier (a compiler-local label)"))
            continue
        # every occurrence is replaced once, all at the same time, so a chain or a swap (OSLink -> Relocate while
        # fn_8009ED4C -> OSLink) is safe; only a new name that stays another, unrenamed address's name would collide
        if nw in b_names and any(b[k] == nw and u.get(k) == nw for k in b_names[nw]):
            skipped.append((old, nw, "new name already names another address that keeps it"))
            continue
        ren[old] = nw
    return ren, skipped


def files(paths):
    for p in paths:
        if os.path.isfile(p):
            yield p
            continue
        for root, dirs, fs in os.walk(p):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
            for f in fs:
                if f.endswith(".inc"):
                    continue
                if f.endswith(EXTS) or f in NAMES:
                    yield os.path.join(root, f)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("base")
    ap.add_argument("new")
    ap.add_argument("paths", nargs="*")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--report")
    ap.add_argument("--map", help="write the lookup table (old name, new name, section, address) as TSV and stop")
    ap.add_argument("--alias-from", metavar="REV",
                    help="alias-header mode: for each path, take its text at REV and rename only the left side of each "
                         "'#define GAME_NAME PC_NAME' line (the right side names Aurora's / the PC's own function and "
                         "never changes); a line whose two sides become equal is turned into a comment")
    a = ap.parse_args()
    ren, skipped = rename_map(a.base, a.new)
    if a.alias_from:
        for path in a.paths:
            text = subprocess.run(["git", "show", f"{a.alias_from}:{path.replace(os.sep, '/')}"], capture_output=True,
                                  check=True).stdout
            out, changed, dropped = [], 0, 0
            for line in text.split(b"\n"):
                m = re.match(rb"(#define\s+)([A-Za-z_]\w*)(\s+)([A-Za-z_]\w*)(.*)$", line)
                if m and m.group(2).decode() in ren:
                    new = ren[m.group(2).decode()].encode()
                    if new == m.group(4):
                        line = (b"/* " + new + b": the game now calls it by this name (upstream rename "
                                b"2026-10-18), the same as its PC target */")
                        dropped += 1
                    else:
                        line = m.group(1) + new + m.group(3) + m.group(4) + m.group(5)
                        changed += 1
                out.append(line)
            if a.write:
                open(path, "wb").write(b"\n".join(out))
            print(f"{path}: {changed} left sides renamed, {dropped} now identical (commented out)")
        return
    if a.map:
        b, _ = symbols(a.base)
        u, _ = symbols(a.new)
        # the permanent, cumulative map (snatcherLeo, 2026-10-18: every upstream merge adds its renames, so old names always
        # stay findable): one row per rename, with the merge it came from; rows already there are kept as they are
        from pathlib import Path
        merge = f"{a.base[:9]}..{a.new[:9]}"
        path = Path(a.map)
        rows = path.read_text(encoding="utf-8").splitlines() if path.exists() else [
            "# Every symbol rename taken from upstream, by merge (apply_renames.py --map; never edit rows by hand).",
            "# old = the placeholder or earlier name, new = upstream's name; the address is what links them. A name",
            "# renamed twice has two rows (follow old -> new -> newer).",
            "old\tnew\tsection\taddress\tmerge"]
        have = {tuple(r.split("\t")[:4]) for r in rows if "\t" in r}
        added = 0
        for (sec, addr), old in sorted(b.items(), key=lambda kv: (kv[0][0], int(kv[0][1], 16))):
            nw = u.get((sec, addr))
            if nw and nw != old and (old, nw, sec, addr) not in have:
                rows.append(f"{old}\t{nw}\t{sec}\t{addr}\t{merge}")
                added += 1
        path.write_text("\n".join(rows) + "\n", encoding="utf-8", newline="\n")
        print(f"{path}: {added} renames added for merge {merge} ({len(rows)} lines)")
        return
    print(f"{len(ren)} renames by address; {len(skipped)} skipped")
    for s in skipped[:20]:
        print("  skipped:", s)
    if not ren:
        return
    names = sorted(ren, key=len, reverse=True)
    pat = re.compile(rb"(?<![A-Za-z0-9_])(" + b"|".join(re.escape(n.encode()) for n in names) + rb")(?![A-Za-z0-9_])")
    table = {k.encode(): v.encode() for k, v in ren.items()}
    changed, total = [], 0
    for path in files(a.paths or DEFAULT_PATHS):
        data = open(path, "rb").read()
        n = [0]

        def sub(m):
            n[0] += 1
            return table[m.group(1)]
        out = pat.sub(sub, data)
        if n[0]:
            changed.append((path, n[0]))
            total += n[0]
            if a.write:
                open(path, "wb").write(out)
    print(f"{'changed' if a.write else 'would change'} {total} identifiers in {len(changed)} files")
    if a.report:
        with open(a.report, "w") as f:
            f.write(f"{len(ren)} renames, {len(skipped)} skipped\n")
            for s in skipped:
                f.write(f"skipped {s[0]} -> {s[1]}: {s[2]}\n")
            for p, k in changed:
                f.write(f"{k}\t{p}\n")


if __name__ == "__main__":
    main()
