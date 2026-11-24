"""Assemble a CLEAN OrrePort release folder: what a tester unzips (2026-11-24; step 5.9's laptop sitting and
release day, 5.11).

    python -I tools/local/port/make_release_folder.py <new folder> [stage_bin.py options, e.g. --from build-pc/firstlink]

Why: the first download (make_release.py --full) must be a folder no one has played in: the launchers, app\\ and the
folders the player fills, nothing personal. snatcherLeo's play folder (decomp\\OrrePort) holds saves, settings and logs, and
make_release.py refuses it. This builds the clean one in three steps, writing only inside the new folder:
  1. port/tools/stage_bin.py stages the current build into <new folder>\\app\\ and puts the two launchers beside it
     (OrrePort.exe, "OrrePort Studio.exe"; the same staging every play build uses);
  2. the empty folders the player fills: "Pokemon Colosseum\\ROM" (the disc) and "Pokemon Colosseum\\ROM Other Regions"
     (a European or Japanese disc for the other languages); everything else (data\\, Saves\\, Reports\\, ...) the app
     makes on its first start, where the portable layout says (docs/port/portable-layout.md);
  3. README.txt from port/release/README.txt, with Windows line ends (Notepad).
The folder must not exist yet (nothing is ever overwritten or deleted). Then make the zip:
    python -I tools/local/port/make_release.py <app folder> <version> <out> --full-only --full <new folder>

Platforms: Windows as written (stage_bin.py is Windows-only: DLL import tables, .exe names). Linux / macOS (likely): the
same three steps with their own staging (an AppImage-style folder, a .app bundle). XD (likely): the same folder, one
app\\ for both games.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
FILL = ("Pokemon Colosseum/ROM", "Pokemon Colosseum/ROM Other Regions")


def main():
    if len(sys.argv) < 2 or sys.argv[1].startswith("-"):
        print(__doc__)
        return 2
    out = os.path.abspath(sys.argv[1])
    if os.path.exists(out):
        print(f"{out} exists already: give a new folder (nothing is overwritten)")
        return 1
    os.makedirs(out)
    r = subprocess.run([sys.executable, "-I", os.path.join(REPO, "port", "tools", "stage_bin.py"), out] + sys.argv[2:])
    if r.returncode != 0 or not os.path.isfile(os.path.join(out, "app", "orrecolo.exe")):
        print("stage_bin.py failed: the folder is incomplete (left as it is, for a look)")
        return 1
    for launcher in ("OrrePort.exe", "OrrePort Studio.exe"):
        if not os.path.isfile(os.path.join(out, launcher)):
            print(f"no {launcher} beside app\\: build the launchers first (tools/local/port/build_launcher.py)")
            return 1
    for d in FILL:
        os.makedirs(os.path.join(out, *d.split("/")))
    text = open(os.path.join(REPO, "port", "release", "README.txt"), encoding="utf-8").read()
    with open(os.path.join(out, "README.txt"), "w", encoding="utf-8", newline="\r\n") as f:
        f.write(text)
    print(f"release folder: {out} (app\\, the launchers, {', '.join(FILL)}, README.txt)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
