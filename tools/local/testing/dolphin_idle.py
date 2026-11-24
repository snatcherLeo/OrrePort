"""Power on and leave the game alone for a set time (testing lane): boot, logos, title, then the attract room.

Runs inside Felk's Dolphin fork:

    set POS_OUT=<folder>   set IDLE_FRAMES=9000   set TESTING_DIR=<repo>/tools/local/testing
    Dolphin.exe -b -C Dolphin.Movie.DumpAudio=True -e "<Colosseum ISO>" --script dolphin_idle.py

Presses nothing; logs every room change (idlog.py, ids.jsonl) and the frame it happened on, and exits after
IDLE_FRAMES frames. Used with Dolphin's audio dump for the PC-vs-console mix comparison (gamecube-accuracy.md entry 10:
level and clipping in the boot-to-attract stretch); the dump lands in the fork's User/Dump/Audio (dspdump.wav,
dtkdump.wav).
"""
import json, os, sys
from dolphin import event

OUT = os.environ.get("POS_OUT", ".")
FRAMES = int(os.environ.get("IDLE_FRAMES", "9000"))
frame = 0
log = open(os.path.join(OUT, "idle.jsonl"), "w", buffering=1)
log.write(json.dumps({"ev": "start", "frames": FRAMES}) + "\n")


def on_frame():
    global frame
    frame += 1
    if frame >= FRAMES:
        log.write(json.dumps({"ev": "end", "frame": frame}) + "\n")
        log.close()
        os._exit(0)


try:  # runtime id log: the rooms passed through (logos, title, attract)
    sys.path.insert(0, os.environ.get("TESTING_DIR", ""))
    import idlog
except Exception as e:
    open(os.path.join(OUT, "idlog_error.txt"), "w").write(repr(e))
    event.on_frameadvance(on_frame)
else:
    idlog.install(event, on_frame, OUT, run="idle")
