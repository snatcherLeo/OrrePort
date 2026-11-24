"""Measure how Felk's Dolphin fork passes StickX / StickY from controller.set_gc_buttons to the game (testing lane).

    set POS_OUT=<folder holding s1_out_arrival.sav>   Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_stickcal.py

Loads the state, then holds each test value for 12 frames (one axis at a time, the other at 128, then both together)
and logs, on the 10th frame, the stick the game read: InputPad slot 0's latched PADStatus (lbl_80401C10 + 0x18; +2 X,
+3 Y, signed; logged +128 so 128 = centre, the same scale dolphin_record.py records). Writes stickcal.jsonl, then exits.
A replay found the values arriving changed (set X 27 -> game 229, set Y 128 -> game 0; 2026-10-17).
"""
import json, os, time
from dolphin import controller, event, memory, savestate

OUT = os.environ.get("POS_OUT", ".")
LATCHED = 0x80401C10 + 0x18
# first pass (2026-10-17, 12 values per axis) showed a value doesn't reach the game unchanged, and that an axis left out
# of the dict arrives centred; so every value is measured with the other axis left out (STICKCAL_FULL=1 = all 256)
CSTICK = os.environ.get("STICKCAL_CSTICK") == "1"   # 2026-10-18: the C-stick (CStickX / CStickY, latched +4 / +5)
if CSTICK:
    TESTS = [{"CStickX": v} for v in range(256)] + [{"CStickY": v} for v in range(256)]
elif os.environ.get("STICKCAL_FULL") == "1":
    TESTS = [{"StickX": v} for v in range(256)] + [{"StickY": v} for v in range(256)] \
        + [{"StickX": v} for v in (0, 27, 128, 229)]          # repeats: is the mapping stable?
else:
    VALS = [0, 1, 27, 64, 100, 127, 128, 129, 156, 200, 229, 255]
    TESTS = [{"StickX": v, "StickY": 128} for v in VALS] + [{"StickX": 128, "StickY": v} for v in VALS] \
        + [{"StickX": 27, "StickY": 27}, {"StickX": 229, "StickY": 229}, {"StickX": 27, "StickY": 229}, {},
           {"StickX": 27}, {"StickY": 27}]
out = open(os.path.join(OUT, "stickcal.jsonl"), "w", buffering=1)
frame = 0
st = {"loaded": False}


def s8(v):
    return v - 256 if v > 127 else v


def on_frame():
    global frame
    frame += 1
    if not st["loaded"]:
        st["loaded"] = True
        savestate.load_from_file(os.path.join(OUT, "s1_out_arrival.sav"))
        return
    i, k = divmod(frame - 30, 12)      # 30 frames to settle after the load
    if i < 0:
        return
    if i >= len(TESTS):
        out.flush()
        time.sleep(1)
        os._exit(0)
    controller.set_gc_buttons(0, TESTS[i])
    if k == 10:
        off = 4 if CSTICK else 2
        x, y = memory.read_u8(LATCHED + off), memory.read_u8(LATCHED + off + 1)
        out.write(json.dumps({"set": TESTS[i], "game": [s8(x) + 128, s8(y) + 128]}) + "\n")


event.on_frameadvance(on_frame)
