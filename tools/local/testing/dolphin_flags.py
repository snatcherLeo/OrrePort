"""Dump the story-flag buffers from a Dolphin save state (testing lane). Runs inside Felk's Dolphin fork:

    set POS_OUT=<folder>   set FLAG_STATE=<state file in that folder, e.g. after_willie_menu.sav>
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_flags.py

Loads the state, waits 60 frames, reads the flag storage (lbl_80478EEC: 4 FlagStateEntry {u32 wordCount, u32 buffer}
for classes 0-3; flags.md), writes <state>.flags.json = {"class": [words...]} and exits. Decode it with
tools/local/testing/flag_report.py. If lbl_80478EEC holds a pointer to the array rather than the array, both are tried.
"""
import json, os
from dolphin import event, memory, savestate

OUT = os.environ.get("POS_OUT", "."); STATE = os.environ["FLAG_STATE"]
STORAGE = 0x80478EEC
frame = 0

def ram(a):
    return 0x80000000 <= a < 0x81800000

def read_entries(base):
    out = {}
    for c in range(4):
        n, buf = memory.read_u32(base + 8 * c), memory.read_u32(base + 8 * c + 4)
        if buf and ram(buf) and 0 < n <= 0x1000:
            out[c] = [memory.read_u32(buf + 4 * i) for i in range(n)]
    return out

def on_frame():
    global frame
    frame += 1
    if frame == 1:
        savestate.load_from_file(os.path.join(OUT, STATE)); return
    if frame < 60:
        return
    res = read_entries(STORAGE)
    p = memory.read_u32(STORAGE)
    if not res and ram(p):
        res = read_entries(p)
    json.dump({"state": STATE, "storage": hex(STORAGE), "classes": {str(k): v for k, v in res.items()}},
              open(os.path.join(OUT, STATE + ".flags.json"), "w"))
    os._exit(0)
event.on_frameadvance(on_frame)
