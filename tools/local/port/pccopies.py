"""Hide unscored copies of functions on the PC (OrrePort; the project notes Main queue "dead copies").

Multi-unit files and candidate units compile copies of functions whose scored copy lives in another unit
(livecheck.py says COPY: no unit that owns the function compiles the line). The GameCube build never links them;
the PC link would weaken them anyway (pcunits.py). When such a copy no longer compiles on the PC (leftover-register
pseudo-C, old declarations), wrapping the whole function in `#ifndef TARGET_PC` is the honest fix: the PC uses the
owner's copy. This tool takes call sites (file:line), checks each with livecheck.py, and wraps the enclosing function
of every COPY site (from its header line to its closing brace). Files that use __LINE__ are skipped (the inserted lines
would shift it). GameCube build unchanged; verify with ninja, 26 SHA-1, fullsnap.

Usage (repo root): python -I tools/local/port/pccopies.py FILE:LINE [...]   (needs gcc on PATH for livecheck)
"""
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import callsite  # noqa: E402


def main():
    sites = sys.argv[1:]
    chk = subprocess.run([sys.executable, "-I", str(Path(__file__).parent / "livecheck.py")] + sites,
                         capture_output=True, text=True).stdout
    copies = {}
    for line in chk.splitlines():
        parts = line.split()
        if len(parts) > 4 and parts[0] == "COPY":
            f, n = parts[1].rsplit(":", 1)
            owner = re.search(r"owner (\S+)", line)
            copies.setdefault(f, {})[int(n)] = owner.group(1) if owner else "?"
    done = 0
    for f, lines in copies.items():
        p = Path(f)
        b = p.read_bytes().decode("utf-8", errors="surrogateescape")
        if "__LINE__" in b:
            print(f"SKIP {f}: uses __LINE__")
            continue
        nl = "\r\n" if "\r\n" in b else "\n"
        L = b.split(nl)
        spans = {}
        for n, owner in lines.items():
            fn, start = callsite.enclosing([x + "\n" for x in L], n)
            if not fn:
                continue
            # find the opening brace and its matching close
            i = start - 1
            depth, opened, end = 0, False, None
            for k in range(i, len(L)):
                for ch in L[k]:
                    if ch == "{":
                        depth += 1
                        opened = True
                    elif ch == "}":
                        depth -= 1
                if opened and depth == 0:
                    end = k
                    break
            if end is not None and end >= n - 1:
                spans[(start - 1, end)] = (fn, owner)
        for (s, e), (fn, owner) in sorted(spans.items(), reverse=True):
            if s > 0 and L[s - 1].startswith("#ifndef TARGET_PC /* copy:"):
                continue
            L[s:e + 1] = [f"#ifndef TARGET_PC /* copy: {fn} is owned and scored in {owner}; the PC links that one */"] + \
                L[s:e + 1] + ["#endif"]
            done += 1
            print(f"wrapped {f}: {fn} (lines {s + 1}-{e + 1}; owner {owner})")
        p.write_bytes(nl.join(L).encode("utf-8", errors="surrogateescape"))
    print(f"{done} copy functions hidden on the PC")
    return 0


if __name__ == "__main__":
    sys.exit(main())
