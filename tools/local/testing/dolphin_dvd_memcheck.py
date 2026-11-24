"""Test: does a memory breakpoint in Felk's fork fire on EVERY write (unlike code breakpoints, which report only their
first hit)? If so, every disc read can be logged, not one per frame (B's disc-model recalibration, 2026-10-19).

    set POS_OUT=<folder>   set MEMCHECK_FRAMES=1500
    Dolphin.exe -b -e "<ISO>" --script dolphin_dvd_memcheck.py

Watches writes to executing_8047A7E8 (the SDK DVD driver's "command block in progress" pointer): each new read sets it.
On each hit it logs the frame, the hit's order within the frame, and the block's disc offset / length. Also logs the
per-frame poll (as dolphin_slice.py's DVD probe does) for comparison. Writes memcheck.jsonl and errors.txt.
"""
import json, os, time, traceback
from dolphin import event, memory

OUT = os.environ.get("POS_OUT", ".")
N = int(os.environ.get("MEMCHECK_FRAMES", "1500"))
EXEC, DUMMY = 0x8047A7E8, 0x803FC3A0
log = open(os.path.join(OUT, "memcheck.jsonl"), "w", buffering=1)
err = open(os.path.join(OUT, "errors.txt"), "w", buffering=1)
st = {"frame": 0, "in_frame": 0, "hits": 0}


def ram(a):
    return 0x80000000 <= a < 0x81800000


def on_mem(*args):
    st["hits"] += 1
    st["in_frame"] += 1
    try:
        b = memory.read_u32(EXEC)
        rec = {"ev": "hit", "frame": st["frame"], "n": st["in_frame"], "args": [repr(x) for x in args], "block": hex(b)}
        if ram(b) and b != DUMMY:
            rec["offset"] = hex(memory.read_u32(b + 0x10))
            rec["length"] = memory.read_u32(b + 0x14)
        log.write(json.dumps(rec) + "\n")
    except Exception:
        err.write(traceback.format_exc())


def on_frame():
    st["frame"] += 1
    st["in_frame"] = 0
    if st["frame"] == 1:
        tried = []
        # the fork's add_memcheck takes plain integers, not a settings dict (first try, 2026-10-19: "'dict' object cannot
        # be interpreted as an integer"); try an address, then a start / end pair
        for args in ((EXEC,), (EXEC, EXEC + 3)):
            try:
                memory.add_memcheck(*args)
                tried.append("ok: " + repr(args))
                break
            except Exception as e:
                tried.append(f"failed {args!r}: {e!r}")
        log.write(json.dumps({"ev": "memcheck", "tried": tried}) + "\n")
    b = memory.read_u32(EXEC)
    if b != st.get("poll"):
        st["poll"] = b
        rec = {"ev": "poll", "frame": st["frame"], "block": hex(b)}
        if ram(b) and b != DUMMY:
            rec["offset"] = hex(memory.read_u32(b + 0x10))
            rec["length"] = memory.read_u32(b + 0x14)
        log.write(json.dumps(rec) + "\n")
    if st["frame"] >= N:
        log.write(json.dumps({"ev": "end", "frame": st["frame"], "hits": st["hits"]}) + "\n")
        log.flush()
        time.sleep(1)
        os._exit(0)


event.on_frameadvance(on_frame)
try:
    event.on_memorybreakpoint(on_mem)
except Exception:
    err.write(traceback.format_exc())
