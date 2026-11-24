"""2026-11-02: runs one the reference playthrough check replay inside saferun (the replay_check --inner pattern): the exe in its
folder, its output to run.log, stopped 600 frames after the pad script's last step, or when no new frame comes for 60 s.
    python -I run3_runner.py <replay folder>
Called by run3_pc_replay.sh (same folder); moved into the repo from a session scratch folder on 2026-11-11.
"""
import os, queue, re, subprocess, sys, threading, time

ANYFRAME = re.compile(r"(?:frame[= ]|game frame )(\d+)")
out = sys.argv[1]
steps = [l for l in open(os.path.join(out, "sitting.pad"), encoding="utf-8") if l[:1].isdigit()]
last = int(steps[-1].split(":", 1)[0]) if steps else 0
stop_at = last + int(os.environ.get("RUN3_TAIL", "600"))  # RUN3_TAIL (2026-11-05): run on past the last step
# RUN3_STOP=<frame> (2026-11-06): stop there instead (a short frame dump early in a long sitting; the rolls then
# compare as a prefix of the live ones)
if os.environ.get("RUN3_STOP"):
    stop_at = int(os.environ["RUN3_STOP"])
log = open(os.path.join(out, "run.log"), "w", encoding="utf-8", buffering=1)
# RUN3_WRAP="prog|arg|arg" (2026-11-04): start the exe through another program, e.g. RenderDoc's renderdoccmd capture
wrap = [w for w in os.environ.get("RUN3_WRAP", "").split("|") if w]
# RUN3_EXE=<path> (2026-11-24): run that exe (a frozen build shared by every sitting of a sweep, run3_pc_replay.sh's
# RUN_FROM) instead of a copy in the replay folder; the replay folder is then only the run's data, through ORRE_ROOT
exe = os.environ.get("RUN3_EXE") or os.path.join(out, "orrecolo.exe")
p = subprocess.Popen(wrap + [exe], cwd=out, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                     text=True, encoding="utf-8", errors="replace", bufsize=1)
lines = queue.Queue()
threading.Thread(target=lambda: [lines.put(l) for l in p.stdout] and None or lines.put(None), daemon=True).start()
cur, last_progress, why = 0, time.time(), "the exe exited"
while True:
    try:
        chunk = lines.get(timeout=1)
    except queue.Empty:
        chunk = ""
    if chunk is None:
        break
    if chunk:
        log.write(chunk)
        for m in ANYFRAME.finditer(chunk):
            n = int(m.group(1))
            # 2026-11-05 (P09 pass M1mem): two threads' log lines can interleave mid-number ("frame=85505" + "6 pos"
            # read as 855056), which ended a replay 48,000 frames early; a frame more than 3,000 past the last one
            # seen is such a splice, not progress (the game logs a frame line at least every 60 frames)
            if cur < n <= cur + 3000 or (cur == 0 and n < 3000):
                cur, last_progress = n, time.time()
        if cur >= stop_at:
            why = f"RUN3_STOP {stop_at}" if os.environ.get("RUN3_STOP") else f"600 frames after the pad script's last step ({last})"
            break
    # RUN3_STALL=<seconds> (2026-11-11): a longer stall limit for a slow renderer (SwiftShader runs on the processor)
    stall = int(os.environ.get("RUN3_STALL", "60"))
    if time.time() - last_progress > stall:
        why = f"stall: no new frame for {stall} s (last frame {cur})"
        break
if p.poll() is None:
    p.kill()
    p.wait()
log.write(f"[run3] stopped at frame {cur}: {why}\n")
log.close()
print(f"[run3] stopped at frame {cur}: {why}")
