"""Hide the local declarations GCC reports as "conflicting types" (OrrePort; the project notes Main queue, compile-error tail).

pcproto.py hides the guessed local declarations of every function that has a canonical prototype
(include/pc/prototypes.h). Some slip through: block-scope `extern T name();` lines reached through an alias macro
(`#define fn_801F025C fightTargetGetPtrAsNowFightType`), or declarations added after pcproto ran. This reads the PC
probe log (build-pc/pc_probe.log), takes each "conflicting types for 'X'" error, and when the reported line is a
one-line declaration (`[extern] T X(...);` or `extern T X[];`) of a name that has a canonical prototype or a
canonical data declaration, wraps that line in `#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */`.
Definitions are never touched. Files that use __LINE__ are skipped (the inserted lines would shift it).
GameCube build unchanged; verify with ninja, 26 SHA-1, fullsnap. Re-run the probe afterwards: the call sites then
use the canonical types, which can surface new (real) errors.

Usage (repo root): python -I tools/local/port/pchide.py [--dry-run] [--log build-pc/pc_probe_alias.log] [--exclude PREFIX ...]

Address-name aliases come from the file itself, port/src/sdk_pc_alias.h and include/pc/game_pc_alias.h
(pc_probe.py --alias). SDK names declared by Aurora's headers count as canonical too.
"""
import re
import sys
from pathlib import Path

ERR = re.compile(r"^(?:\S+: )?(\.?/?[\w/.-]+\.(?:c|cpp|h)):(\d+):\d+: error: conflicting types for '(\w+)'")
EXCLUDE = ()
MARK = "#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */"


def canonical_names():
    t = Path("include/pc/prototypes.h").read_text(encoding="utf-8")
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)  # comments hold "(...)" that would swallow the next name
    names = set(re.findall(r"\b(\w+)\s*\([^;{}]*\)\s*;", t))
    names |= set(re.findall(r"\b(\w+)\s*\[[^\]]*\]\s*;", t))
    for h in Path("extern/aurora/include/dolphin").rglob("*.h"):  # SDK names: Aurora's declarations are the canonical ones
        a = re.sub(r"/\*.*?\*/", "", h.read_text(encoding="utf-8", errors="replace"), flags=re.S)
        names |= set(re.findall(r"\b(\w+)\s*\([^;{}]*\)\s*;", a))
    return names


def main():
    dry = "--dry-run" in sys.argv
    global EXCLUDE
    EXCLUDE = tuple(sys.argv[i + 1] for i, x in enumerate(sys.argv) if x == "--exclude")  # another session's files
    canon = canonical_names()
    sites = {}
    log = sys.argv[sys.argv.index("--log") + 1] if "--log" in sys.argv else "build-pc/pc_probe.log"
    galias = {}
    for hp in ("port/src/sdk_pc_alias.h",):  # the project's SDK address names, force-included before the game aliases
        if Path(hp).exists():
            galias.update(re.findall(r"^#define (fn_[0-9A-F]{8}) (\w+)", Path(hp).read_text(encoding="utf-8"), re.M))
    if Path("include/pc/game_pc_alias.h").exists():
        galias.update(re.findall(r"^#define (fn_[0-9A-F]{8}) (\w+)$", Path("include/pc/game_pc_alias.h").read_text(encoding="utf-8"), re.M))
    for line in Path(log).read_text(encoding="utf-8", errors="replace").splitlines():
        m = ERR.match(line)
        if m:
            f, n = m.group(1).lstrip("./"), int(m.group(2))
            at = re.search(r"\[at \.?/?([\w/.-]+):(\d+)\]", line)  # error inside a macro: the real site
            if at:
                f, n = at.group(1).lstrip("./"), int(at.group(2))
            sites.setdefault(f, {})[n] = m.group(3)
    done = skipped = 0
    for f, lines in sorted(sites.items()):
        p = Path(f)
        if not p.exists() or not f.startswith(("src/", "include/")) or f.startswith(EXCLUDE):
            continue
        b = p.read_bytes().decode("utf-8", errors="surrogateescape")
        if "__LINE__" in b:
            print(f"SKIP {f}: uses __LINE__")
            skipped += len(lines)
            continue
        nl = "\r\n" if "\r\n" in b else "\n"
        L = b.split(nl)
        aliases = dict(galias)
        aliases.update(re.findall(r"^\s*#\s*define\s+(\w+)\s+(\w+)\s*$", b, re.M))
        for n in sorted(lines, reverse=True):
            name = lines[n]
            src = L[n - 1]
            real = {name, aliases.get(name, name)}
            decl = re.match(r"^\s*(?:extern\s+)?[\w\s\*]+?\b(\w+)\s*(\([^;{}]*\)|\[[^\]]*\])\s*;\s*(/\*.*\*/)?\s*$", src)
            if decl and aliases.get(decl.group(1)) == name:  # the line says fn_X, the error names the real function
                real.add(decl.group(1))
            if not decl or decl.group(1) not in real or not (real & canon) or src.lstrip().startswith(("return", "typedef")):
                print(f"leave {f}:{n}: {src.strip()[:90]}")
                skipped += 1
                continue
            if n >= 2 and L[n - 2].strip() == MARK:
                continue
            if not dry:
                L[n - 1:n] = [MARK, src, "#endif"]
            done += 1
            print(f"hid {f}:{n}: {src.strip()[:90]}")
        if not dry:
            p.write_bytes(nl.join(L).encode("utf-8", errors="surrogateescape"))
    print(f"{done} declarations hidden on the PC, {skipped} left for a hand fix")
    return 0


if __name__ == "__main__":
    sys.exit(main())
