"""Freeze the current PC build once for a sweep (2026-11-24; the ORRE_ROOT test switch).

    python -I tools/local/testing/freeze_build.py [<dest folder>]

Copies the program files of build-pc/firstlink (orrecolo.exe, its DLLs, the UI and Fonts folders and the pipeline seed
initial_pipeline_cache.db) into one folder, by default decomp/_run3/frozen/<date>_<time>_<commit>/, and prints its
path. A sweep then runs every sitting from that one folder (run3_pc_replay.sh RUN_FROM=<it>), each sitting's data in
its own check folder through ORRE_ROOT.

Why a frozen copy, and only one: build-pc/firstlink is relinked in place, so a sweep running it directly would change
program halfway through when a build lands; a copy per run (the old way) filled the disk (the 5.5 clean house found
the sessions' 466 GB, mostly exe copies) and gave Windows a new program location to ask about each time. One copy per
sweep keeps a sweep's program fixed, costs ~300 MB once, and is one firewall entry at most. Delete the folder when the
sweep's results are written down (it is a build output).

Also writes FROZEN.txt beside the files: the commit, the exe's SHA-1 and when it was frozen, so a result can always
say which program it came from. Plain Python; Windows paths in practice (the build is Windows-first), the same idea on
Linux and macOS with their binary's name (likely).
"""
import datetime
import hashlib
import os
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SRC = os.path.join(REPO, "build-pc", "firstlink")
FILES = ("orrecolo.exe", "initial_pipeline_cache.db")
FOLDERS = ("ui", "Fonts")


def main():
    exe = os.path.join(SRC, "orrecolo.exe")
    if not os.path.isfile(exe):
        print(f"no build at {exe}: build first (ninja -C build-pc, then port/tools/firstlink.py)")
        return 1
    commit = subprocess.run(["git", "-C", REPO, "rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip()
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    dest = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(REPO), "_run3", "frozen", f"{stamp}_{commit}")
    if os.path.exists(dest):
        print(f"{dest} exists: give another folder")
        return 1
    os.makedirs(dest)
    for f in FILES:
        if os.path.isfile(os.path.join(SRC, f)):
            shutil.copy2(os.path.join(SRC, f), dest)
    for f in os.listdir(SRC):
        if f.lower().endswith(".dll"):
            shutil.copy2(os.path.join(SRC, f), dest)
    for d in FOLDERS:
        if os.path.isdir(os.path.join(SRC, d)):
            shutil.copytree(os.path.join(SRC, d), os.path.join(dest, d))
    sha = hashlib.sha1(open(os.path.join(dest, "orrecolo.exe"), "rb").read()).hexdigest()
    with open(os.path.join(dest, "FROZEN.txt"), "w", encoding="utf-8") as f:
        f.write(f"frozen {stamp} from build-pc/firstlink\ncommit {commit}\norrecolo.exe sha1 {sha}\n")
    print(dest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
