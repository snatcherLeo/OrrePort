"""Boot a portable OrrePort folder and check what it writes (Phase 4 step 4.1; docs/port/portable-layout.md part 7,
step 2's check).

    python -I tools/local/port/portable_check.py <OrrePort folder> [--frames 1500] [--seconds 120] [--pad FILE]
        [--env NAME=VALUE ...] [--log FILE]

Why: the portable layout promises that app\\ is never written and that everything the game writes lands in
data\\settings\\, data\\system\\ or Pokemon Colosseum\\ (the project notes section 8; layout A since 2026-11-22,
snatcherLeo: bin\\, settings\\ and system\\ before). This runs app\\orrecolo.exe the way the launcher
will (working folder = the OrrePort folder) with **no MSYS2 on PATH** (only Windows' own folders), so a DLL missing
from app\\ stops the start, and lists every file the run added or changed, grouped by top folder. It fails when:
the exe can't start, anything under app\\ was added or changed, the log lacks the "[paths] portable layout" line, or a
file was written outside the three folders above.

The run is an automated one (ORRE_WINDOW=minimized, so the hub is skipped and live-play recording is off); its own
console output goes to --log (default: beside the OrrePort folder, never inside it, so the listing stays clean). It
stops at --frames (a "frame N" line in the output), at --seconds, or when no output comes for 60 s (the stall check), and
the process is then ended (scratch folders only: never point this at snatcherLeo's play folder while he plays; run it through
tools/local/saferun.py).

Platforms: Windows as written (orrecolo.exe, the PATH of Windows' own folders). Linux / macOS (likely): the same check
with app/orrecolo and a PATH / library path without the build tools.
"""
import argparse
import os
import queue
import re
import subprocess
import sys
import threading
import time

ANYFRAME = re.compile(r"(?:frame[= ]|game frame )(\d+)")
# Compared with the names on disk exactly, capitals included: Windows would accept a folder made as "Settings", but a
# Linux build wouldn't find it, so a top folder with the wrong capitals fails the check too (snatcherLeo, 2026-11-22).
ALLOWED_TOP = ("data", "Pokemon Colosseum")
ALLOWED_IN_DATA = ("settings", "system")  # layout A: data\ holds only these two (2026-11-22)


def listing(root):
    out = {}
    for dirpath, _dirs, files in os.walk(root):
        for name in files:
            p = os.path.join(dirpath, name)
            try:
                st = os.stat(p)
            except OSError:
                continue
            out[os.path.relpath(p, root)] = (st.st_size, st.st_mtime_ns)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("root")
    ap.add_argument("--frames", type=int, default=1500)
    ap.add_argument("--seconds", type=int, default=120)
    ap.add_argument("--pad", help="a pad script file (ORRE_PAD_SCRIPT=@file:...)")
    ap.add_argument("--env", nargs="*", default=[], help="extra NAME=VALUE for the run")
    ap.add_argument("--log", help="the run's console output (default: <root>_run.log beside the folder)")
    # --disc (2026-11-24, the release zip's check): the disc image by path (the game's own --disc), read in place, for a
    # fresh folder whose ROM\ is empty; finding a disc in ROM\ is what the hand sitting tests
    ap.add_argument("--disc", help="a disc image to start with (passed as --disc; nothing is copied)")
    a = ap.parse_args()
    root = os.path.abspath(a.root)
    exe = os.path.join(root, "app", "orrecolo.exe")
    if not os.path.isfile(exe):
        sys.exit(f"no {exe} (stage it first: python -I port/tools/stage_bin.py {root})")
    log_path = a.log or root.rstrip("\\/") + "_run.log"

    sysroot = os.environ.get("SystemRoot", r"C:\Windows")
    env = {k: v for k, v in os.environ.items() if not k.startswith("ORRE_")}
    env["PATH"] = os.pathsep.join([os.path.join(sysroot, "System32"), sysroot, os.path.join(sysroot, "System32", "Wbem")])
    env["ORRE_WINDOW"] = "minimized"
    env["ORRE_RUN_WHY"] = "portable layout check (4.1)"
    if a.pad:
        env["ORRE_PAD_SCRIPT"] = "@file:" + os.path.abspath(a.pad).replace("\\", "/")
    for kv in a.env:
        k, _, v = kv.partition("=")
        env[k] = v

    before = listing(root)
    log = open(log_path, "w", encoding="utf-8", buffering=1)
    log.write(f"[portable_check] PATH={env['PATH']}\n")
    p = subprocess.Popen([exe] + (["--disc", os.path.abspath(a.disc)] if a.disc else []), cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                         encoding="utf-8", errors="replace", bufsize=1)
    lines = queue.Queue()
    threading.Thread(target=lambda: [lines.put(l) for l in p.stdout] and None or lines.put(None), daemon=True).start()
    t0 = last = time.time()
    cur, why, text = 0, "the exe exited", []
    while True:
        try:
            chunk = lines.get(timeout=1)
        except queue.Empty:
            chunk = ""
        if chunk is None:
            break
        if chunk:
            last = time.time()
            log.write(chunk)
            text.append(chunk)
            for m in ANYFRAME.finditer(chunk):
                n = int(m.group(1))
                if cur < n <= cur + 3000 or (cur == 0 and n < 3000):
                    cur = n
            if cur >= a.frames:
                why = f"reached frame {cur}"
                break
        if time.time() - t0 > a.seconds:
            why = f"{a.seconds} s passed (frame {cur})"
            break
        if time.time() - last > 60:
            why = f"stall: no output for 60 s (frame {cur})"
            break
    if p.poll() is None:
        p.kill()
        p.wait()
    code = p.returncode
    log.write(f"[portable_check] stopped: {why}; exit code {code}\n")
    log.close()
    time.sleep(1)
    after = listing(root)

    added = sorted(k for k in after if k not in before)
    changed = sorted(k for k in after if k in before and after[k] != before[k])
    problems = []
    joined = "".join(text)
    if "[paths] portable layout" not in joined:
        problems.append('the log has no "[paths] portable layout" line (did the exe start? a DLL missing from app\\?)')
    for k in added + changed:
        top = k.split(os.sep, 1)[0]
        if top == "app":
            problems.append(f"written in app\\: {k}")
        elif top not in ALLOWED_TOP:
            problems.append(f"written outside data\\ and Pokemon Colosseum\\ (exact capitals): {k}")
        elif top == "data" and k.split(os.sep)[1:2] and k.split(os.sep)[1] not in ALLOWED_IN_DATA:
            problems.append(f"written in data\\ outside settings\\ and system\\ (exact capitals): {k}")

    print(f"[portable_check] {why}; exit code {code}; log {log_path}")
    for title, items in (("added", added), ("changed", changed)):
        groups = {}
        for k in items:
            parts = k.split(os.sep)
            groups.setdefault(os.sep.join(parts[:2]) if len(parts) > 2 else parts[0], []).append(k)
        print(f"  {title}: {len(items)} file(s)")
        for g in sorted(groups):
            sample = groups[g][0] if len(groups[g]) == 1 else f"{len(groups[g])} files, e.g. {groups[g][0]}"
            print(f"    {g}: {sample}")
    for want in ("[paths]", "[card] saves", "replay clock"):
        hit = next((l.strip() for l in joined.splitlines() if want in l), None)
        if hit:
            print(f"  log: {hit[:220]}")
    if problems:
        print("FAIL:")
        for pr in problems:
            print(f"  {pr}")
        sys.exit(1)
    print("PASS: nothing written in app\\, everything in data\\settings\\, data\\system\\ or Pokemon Colosseum\\")


if __name__ == "__main__":
    main()
