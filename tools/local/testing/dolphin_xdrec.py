"""XD's sitting recorder and replay watcher, Dolphin side (2026-11-24). Runs inside Felk's Dolphin scripting fork;
started by `playrec.py --game xd`, never by hand.

Why a separate script: snatcherLeo's XD Dolphin-recordings exception (the project rules, "A second exception", 2026-11-24: XD sittings
recorded in Dolphin as the future XD port's reference; no XD decomp, no XD code in the port). Colosseum's recorder
(dolphin_playrec.py) reads Colosseum's own memory addresses for nearly everything: the movie check starts at
Colosseum's logo room, the mark / end combos read the game's copy of the pad, and the room, battle and save-block
events all read Colosseum globals. With XD loaded those would never fire, or log garbage. This script reads **no game
address at all**, so it is the same for any GameCube game (likely):
  - the combos come from Dolphin's own controller state (controller.get_gc_buttons), not from game memory. Its
    buttons are right, its stick isn't (it read 0 / 0 while Wes walked; verified on Colosseum, dolphin_record.py), so
    pad.jsonl is a buttons-only backup: the Dolphin movie is the reference;
  - the movie check is by time after boot, not by room;
  - "the same result" is proved without addresses: replays hash all 24 MB of main RAM every PLAYREC_HASH_EVERY frames
    (default 3,600, one minute) and at the end (hashes.csv); two replays of one movie must give the same hashes.
Room, battle and flag events, checkpoints and memory watches come later, by replaying the movies once XD's addresses
are known (the Colosseum method, verified there; each later watch checked against a plain replay first, since a watch
once disturbed a Colosseum replay).

Modes (PLAYREC_MODE):
- **record**: snatcherLeo plays. Every pad change goes to pad.jsonl (Dolphin's view of controller 1, a backup only: the
  Dolphin movie is the reference). States: every PLAYREC_PERIODIC frames (5 minutes), at each mark (L + R + Z) and at
  the end (L + R + START, then Dolphin closes after 3 s). From 900 frames on, every 1,800 frames until confirmed, a
  probe state checks that Dolphin's movie is recording ("Movie OK", or a red "NO MOVIE" on screen).
- **watch**: a movie replay (`-m`); logs only, touches nothing; hashes RAM; ends at PLAYREC_FRAMES.

Platforms: runs inside Dolphin's embedded Python; nothing OS-specific here (playrec.py handles the Windows parts).
"""
import hashlib, json, os, time
from dolphin import controller, event, memory, savestate
try:
    from dolphin import gui
except Exception:
    gui = None

OUT = os.environ["PLAYREC_OUT"]
MODE = os.environ.get("PLAYREC_MODE", "record")
START_STATE = os.environ.get("PLAYREC_STATE", "")
WATCH_FRAMES = int(os.environ.get("PLAYREC_FRAMES", "0"))           # watch: stop after this many frames (0 = never)
PERIODIC = int(os.environ.get("PLAYREC_PERIODIC", str(5 * 60 * 60)))  # record: frames between periodic states
HASH_EVERY = int(os.environ.get("PLAYREC_HASH_EVERY", "3600"))       # watch: frames between RAM hashes (0 = end only)
RAM_BASE, RAM_SIZE = 0x80000000, 0x01800000                          # the GameCube's 24 MB of main RAM

# the combos, by Dolphin's button names (the same names dolphin_playrec.py sends with set_gc_buttons)
MARK = ("L", "R", "Z")
END = ("L", "R", "Start")
PAD_KEYS = ("A", "B", "X", "Y", "Z", "Start", "Up", "Down", "Left", "Right", "L", "R")

os.makedirs(os.path.join(OUT, "states"), exist_ok=True)
ev_log = open(os.path.join(OUT, "events.jsonl" if MODE == "record" else "replay_events.jsonl"), "a")
pad_log = open(os.path.join(OUT, "pad.jsonl"), "a") if MODE == "record" else None
hash_log = None
if MODE == "watch":
    hash_log = open(os.path.join(OUT, "hashes.csv"), "w")
    hash_log.write("frame,sha1\n")
frame = 0
t0 = time.time()
st = {"started": False, "pad": None, "marks": 0, "end_at": None, "last_state": 0, "movie_ok": False,
      "movie_probe": None, "hash_err": False}


def w(ev, **kw):
    kw.update(f=frame, ev=ev)
    ev_log.write(json.dumps(kw) + "\n")
    ev_log.flush()


def osd(text, color=None, ms=3000):
    try:
        if gui:
            if color is None:
                gui.add_osd_message(text, ms)
            else:
                try:
                    gui.add_osd_message(text, ms, color)
                except TypeError:                     # a build whose call takes no colour
                    gui.add_osd_message(text, ms)
    except Exception:
        pass


def save_state(name, why):
    try:
        savestate.save_to_file(os.path.join(OUT, "states", name + ".sav"))
        st["last_state"] = frame
        w("state", name=name, why=why)
    except Exception as e:
        w("state_error", name=name, err=repr(e))


def ram_sha1():
    """SHA-1 of all main RAM. memory.read_bytes where the fork has it; the pinned build has none (verified on
    Colosseum: dolphin_playrec.py's save-block probe), so 64-bit reads, about 3 million per hash (seconds each; guess
    until timed on the test sitting). The emulator waits for the frame hook, so a slow hash only slows the replay."""
    h = hashlib.sha1()
    if hasattr(memory, "read_bytes"):
        step = 0x100000
        for a in range(RAM_BASE, RAM_BASE + RAM_SIZE, step):
            h.update(bytes(memory.read_bytes(a, step)))
    elif hasattr(memory, "read_u64"):
        for a in range(RAM_BASE, RAM_BASE + RAM_SIZE, 8):
            h.update(memory.read_u64(a).to_bytes(8, "big"))
    else:
        for a in range(RAM_BASE, RAM_BASE + RAM_SIZE, 4):
            h.update(memory.read_u32(a).to_bytes(4, "big"))
    return h.hexdigest()


def write_hash():
    try:
        hash_log.write("%d,%s\n" % (frame, ram_sha1()))
        hash_log.flush()
    except Exception as e:
        if not st["hash_err"]:
            st["hash_err"] = True
            w("hash_error", err=repr(e))


def movie_check():
    """Is Dolphin's movie recording? While one records, Dolphin writes the movie so far beside every state
    (<state>.sav.dtm; verified on Colosseum, 2026-10-18). By time, since this script knows no XD room: first at frame
    900 (15 s, past the boot, where a movie started on Colosseum once froze Dolphin), then every 1,800 frames until
    found."""
    if st["movie_ok"]:
        return
    pending = st["movie_probe"]
    if pending and frame >= pending[1]:
        dtm = os.path.join(OUT, "states", pending[0] + ".sav.dtm")
        st["movie_probe"] = None
        if os.path.exists(dtm) and os.path.getsize(dtm) > 0x100:
            st["movie_ok"] = True
            w("movie_ok", state=pending[0])
            osd("Movie OK")
        else:
            w("no_movie", state=pending[0])
            osd("NO MOVIE: Movie -> Start Recording Input now (or L+R+START and start again)", color=0xFFFF4040,
                ms=15000)
    elif not pending and frame >= 900 and (frame - 900) % 1800 == 0:
        name = "movie_check_%03d" % ((frame - 900) // 1800 + 1)
        save_state(name, "movie check")
        st["movie_probe"] = (name, frame + 30)       # Dolphin writes the state (and the movie) a moment later


def pad_now():
    try:
        d = controller.get_gc_buttons(0)
    except Exception:
        return None
    return {k: d[k] for k in PAD_KEYS if k in d}


def held(p, combo):
    return p is not None and all(p.get(k) for k in combo)


def finish(reason):
    if MODE == "watch":
        write_hash()
    w("end", reason=reason, frames=frame)
    json.dump({"last_frame": frame, "reason": reason, "mode": MODE, "marks": st["marks"], "game": "xd",
               "seconds": round(time.time() - t0, 1)},
              open(os.path.join(OUT, "sitting.json" if MODE == "record" else "replay.json"), "w"))
    ev_log.flush()
    if pad_log:
        pad_log.flush()
    if hash_log:
        hash_log.flush()
    time.sleep(2)
    os._exit(0)


def on_frame():
    global frame
    frame += 1
    if frame % 300 == 1:
        # the heartbeat for playrec.py's no-progress watchdog (as dolphin_playrec.py): a dead script or a stuck
        # emulator stops it, and the watchdog ends the run with the reason logged
        try:
            with open(os.path.join(OUT, "heartbeat.txt"), "w") as hb:
                hb.write("%d %.0f\n" % (frame, time.time()))
        except Exception:
            pass
    if not st["started"]:
        st["started"] = True
        if START_STATE:
            try:
                savestate.load_from_file(START_STATE)
                w("loaded", state=os.path.basename(START_STATE))
            except Exception as e:
                w("load_error", err=repr(e))
        else:
            w("power_on")
        w("script", name="dolphin_xdrec", read_bytes=hasattr(memory, "read_bytes"))
        if MODE == "record":
            osd("Recording XD. L+R+Z = mark, L+R+START = end the sitting", ms=8000)
        return
    if MODE == "record":
        movie_check()
        p = pad_now()
        if p is not None and p != st["pad"]:
            pad_log.write(json.dumps({"f": frame, "p": p}) + "\n")
            if held(p, MARK) and not held(st["pad"], MARK):
                st["marks"] += 1
                w("mark", n=st["marks"])
                save_state("mark_%03d" % st["marks"], "mark")
                osd("Mark %d saved" % st["marks"])
            if held(p, END) and not held(st["pad"], END) and st["end_at"] is None:
                save_state("end", "end of sitting")
                osd("Sitting ended: saving, Dolphin closes in 3 s")
                st["end_at"] = frame + 180
            st["pad"] = p
        if frame - st["last_state"] >= PERIODIC:
            save_state("t%07d" % frame, "periodic")
    elif MODE == "watch":
        # a hash every HASH_EVERY frames, so a difference between two replays is placed within that many frames
        if HASH_EVERY and frame % HASH_EVERY == 0:
            write_hash()
        if WATCH_FRAMES and frame >= WATCH_FRAMES:
            finish("watch complete")
    if st["end_at"] is not None and frame >= st["end_at"]:
        finish("end combo")


event.on_frameadvance(on_frame)
