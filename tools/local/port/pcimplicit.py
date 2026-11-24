"""List the functions PC game code calls with no declaration in scope (OrrePort; upstream merge gate step).

Without a declaration, C assumes `int f()`: a float result comes back as a truncated whole number and a pointer result
loses its upper 32 bits, yet the unit still compiles, links and boots (the PC build passes -w). The 2026-10-18 upstream
merge hid the collision code's only declaration of PSVECMag: vector lengths became whole numbers and Outskirt Stand
lost its walls and exit trigger on the PC, with nothing in any build log.

This compiles every unit firstlink.py compiles (syntax only, the same flags without -w) and collects GCC's
"implicit declaration of function" warnings. The known ones, all integer results, are listed in
pcimplicit_baseline.txt (file and function names only). Anything not in it fails the check: give the function a
declaration (include/pc/prototypes.h for names used in many files), or, when the result really is an int or unused,
add the line to the baseline with --update.

Usage (repo root, MSYS2 mingw64 on PATH; through tools/local/saferun.py):
  python -I tools/local/port/pcimplicit.py [--jobs N] [--update]
"""
import concurrent.futures
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path("port/tools").resolve()))
import firstlink as fl  # noqa: E402

BASELINE = Path("tools/local/port/pcimplicit_baseline.txt")
PAT = re.compile(r"^(\S+?):\d+:\d+: warning: implicit declaration of function '(\w+)'")
FORCED = ("-include", "port/src/sdk_pc_alias.h", "include/pc/game_pc_alias.h", "include/pc/prototypes.h")


def scan_one(unit):
    src, defs, lang = unit
    if lang != "c":  # C++ has no implicit declarations (a missing one is a compile error firstlink reports)
        return []
    flags = [f for f in fl.FLAGS if f not in ("-w", "-c", "-g")]
    if src in fl.SDK_UNITS:  # the same header choice as firstlink.compile_one
        flags = ["-Iport/sdk_include"] + [f for f in flags if f not in FORCED]
    cmd = ["gcc", "-fsyntax-only", "-Wimplicit-function-declaration"] + flags + ["-x", "c", "-std=gnu11"] + \
        [f"-D{d}" for d in defs] + [src]
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    return [f"{src}\t{m.group(2)}" for m in map(PAT.match, r.stderr.splitlines()) if m]


def main():
    jobs = int(sys.argv[sys.argv.index("--jobs") + 1]) if "--jobs" in sys.argv else 12
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        found = sorted(set(row for rows in ex.map(scan_one, fl.units()) for row in rows))
    head = ["# Known implicit function declarations in PC game code (pcimplicit.py): integer or unused results only.",
            "# file<TAB>function. Regenerate with --update after checking each new line's real return type."]
    if "--update" in sys.argv:
        BASELINE.write_text("\n".join(head + found) + "\n", encoding="utf-8")
        print(f"pcimplicit: baseline written, {len(found)} entries")
        return 0
    known = set()
    if BASELINE.exists():
        known = {l for l in BASELINE.read_text(encoding="utf-8").splitlines() if l and not l.startswith("#")}
    new = [r for r in found if r not in known]
    gone = sorted(known - set(found))
    print(f"pcimplicit: {len(found)} implicit declarations, {len(new)} new, {len(gone)} no longer present")
    for r in new:
        print("  NEW  " + r.replace("\t", "  "))
    for r in gone:
        print("  gone " + r.replace("\t", "  "))
    return 1 if new else 0


if __name__ == "__main__":
    sys.exit(main())
