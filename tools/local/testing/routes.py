"""Boot-route checks for the PC build (testing lane; docs/testing/README.md).

Each route runs orrecolo.exe once through saferun (one heavy run at a time; it queues behind the lock) in a fresh
folder under --work (keep it outside the repo: frame dumps are game images), with scripted controller input
(ORRE_PAD_SCRIPT) and frame dumps (ORRE_DUMP_FRAMES). It then checks the route's expectations and prints one verdict
line. Optional: --reference <earlier run folder of the same route> compares the frame dumps (colo/png.compare);
--save <.gci> reads a saved game for the story meter (colo/savefile.py).

    python -I tools/local/testing/routes.py --exe <orrecolo.exe> --disc <image> --work <scratch dir> [route ...]

Route fields: pad (ORRE_PAD_SCRIPT, `frame:BUTTONS` with `+` joins and `-` releases), frames (dump list), seconds
(saferun --time), expect (game-log substrings that must appear), forbid (substrings that must not)."""
import argparse, json, os, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
CRASH = ["[crash]", "[pc_crash] watchdog", "[pc_crash]", "Unhandled exception", "EXCEPTION_"]
ROUTES = {
    # frame numbers are game frames; the inputs follow the project notes section 0 (title -> START -> memory card -> top menu)
    "title": dict(pad="", frames="6000", seconds=150, expect=[], forbid=CRASH),
    # verified 2026-10-16 (B's 13:18 build): frame 4000 shows "The Memory Card in Slot A has been read!"
    "memory_card": dict(pad="100:-,2500:START,2512:-", frames="3000,4000", seconds=150, expect=[], forbid=CRASH),
    # A on the card message, then the top menu (timing still to tune)
    # the project's sequence (2026-10-16, timings guessed): card message -> New Game -> Up + A on the yes / no box (starts on NO)
    # -> Down + A picks WES in the name list -> A on "Is WES OK?" (starts on YES). Expect the top menu (0x3A1), then the
    # opening (0x3A0); sequence verified by Main on build 9e01c2cff
    "new_game": dict(pad="100:-,2500:START,2512:-,4100:A,4112:-,4700:A,4712:-,5200:UP,5212:-,5300:A,5312:-,"
                         "5800:DOWN,5812:-,5950:A,5962:-,6300:A,6312:-",
                     frames="4900,5400,6100,6600", seconds=200,
                     expect=["[room] request 0x3A1", "[room] request 0x3A0"], forbid=CRASH),  # name entry is a pushed menu room: no 0x390 line (verified on 9e01c2cff)
    # the same inputs with the project's test switch ORRE_SKIP_MOVIES=1 (no movie opens; the GameCube path is unchanged), to go
    # past the opening movie into Outskirt Stand (room 1) while the movie's audio stream still crashes (2026-10-16)
    "new_game_skip": dict(pad="100:-,2500:START,2512:-,4100:A,4112:-,4700:A,4712:-,5200:UP,5212:-,5300:A,5312:-,"
                              "5800:DOWN,5812:-,5950:A,5962:-,6300:A,6312:-",
                          frames="6600,7500,9000,11000,13000", seconds=300, env={"ORRE_SKIP_MOVIES": "1"},
                          expect=["[room] request 0x3A1", "[room] request 0x3A0"], forbid=CRASH),
    # new game -> Outskirt Stand -> the diner -> back out -> Willie's battle -> after-battle talk -> pause menu: the
    # Dolphin walk's own inputs, recorded and converted to a room-keyed pad script (pad_to_script.py; B's @file / @R+N /
    # stick steps). Needs a build with B's pad-script extensions (2026-10-16). Expect the battle room (0xC8) and the
    # flags of route steps 1-6 in flags_end.bin.
    "willie_walk": dict(pad="@file:" + os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(HERE))),
                                                    "docs", "testing", "routes", "willie_walk.pad"),
                        frames="", seconds=900, expect=["[room] request 0x1", "[room] request 0x2"], forbid=CRASH),
    # the same replay with frame dumps around the walk's start (room 1 at about frame 10305, first step at +900)
    "willie_walk_frames": dict(pad="@file:" + os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(HERE))),
                                                           "docs", "testing", "routes", "willie_walk.pad"),
                               frames="11150,11250,11350,11450,11600,11800,12000,12300,12600,13000,13500", seconds=300,
                               expect=["[room] request 0x1"], forbid=CRASH),
    "top_menu": dict(pad="100:-,2500:START,2512:-,4100:A,4112:-", frames="4600,5200", seconds=150, expect=[], forbid=CRASH),
}


def run(route, cfg, exe, disc, work, saferun, mem_gb=8):
    d = os.path.abspath(os.path.join(work, f"{route}_{time.strftime('%Y%m%d_%H%M%S')}")); os.makedirs(d)
    saferun = os.path.abspath(saferun)  # the run's working folder is d, so every path must be absolute
    env = dict(os.environ)
    if cfg["frames"]: env["ORRE_DUMP_FRAMES"] = cfg["frames"]
    if cfg["pad"]: env["ORRE_PAD_SCRIPT"] = cfg["pad"]
    env.update(cfg.get("env", {}))
    env["ORRE_DUMP_FLAGS"] = "1"   # B's flag dump: flags_end.bin in the run folder (every 600 frames, at exit and crash)
    log = os.path.join(d, "run.log"); t0 = time.time()
    rc = subprocess.run([sys.executable, "-I", saferun, "--time", str(cfg["seconds"]), "--mem-gb", str(mem_gb), "--log", log,
                         "--", os.path.abspath(exe), "--disc", os.path.abspath(disc)], cwd=d, env=env).returncode
    text = open(log, encoding="utf-8", errors="replace").read() if os.path.exists(log) else ""
    missing = [e for e in cfg["expect"] if e not in text]; found = [f for f in cfg["forbid"] if f in text]
    fdir = os.path.join(d, "frames"); frames = sorted(os.listdir(fdir)) if os.path.isdir(fdir) else []
    want = [x for x in cfg["frames"].split(",") if x]
    # saferun exits 124 when the time limit stops the game (normal: the game never exits by itself); anything else
    # non-zero is a crash or an abort (0xC0000005 = access violation)
    # story-meter inputs from the project's PC-only log lines (2026-10-16): "[room] request 0x%X", "[battle] start %u",
    # "[battle] result %u"
    import re
    rooms = [int(m, 16) for m in re.findall(r"\[room\] request 0x([0-9A-Fa-f]+)", text)]
    battles = [int(m) for m in re.findall(r"\[battle\] start (\d+)", text)]
    results = [int(m) for m in re.findall(r"\[battle\] result (\d+)", text)]
    crashed = rc not in (0, 124)
    if crashed: found.append(f"exit code {rc & 0xFFFFFFFF:#x}")
    return dict(route=route, ok=not crashed and not missing and not found and len(frames) >= len(want), rc=rc,
                seconds=round(time.time() - t0), missing=missing, forbidden=found, frames=frames,
                rooms_requested=[f"{r:#x}" for r in rooms], battles_started=battles, battle_results=results, folder=d)


def compare_frames(run_dir, ref_dir, tolerance=8):
    """{frame file: compare result} for the frames both runs dumped."""
    from colo import png
    a, b = os.path.join(run_dir, "frames"), os.path.join(ref_dir, "frames"); out = {}
    for f in sorted(set(os.listdir(a)) & set(os.listdir(b))) if os.path.isdir(a) and os.path.isdir(b) else []:
        out[f] = png.compare(open(os.path.join(a, f), "rb").read(), open(os.path.join(b, f), "rb").read(), tolerance)
    return out


def story_meter(save_path, disc_path):
    """Story-meter inputs from a saved game (colo/savefile.py)."""
    import struct
    from colo import savefile, world
    from colo.disc import Disc
    from colo.tables import common_rel, table_rows
    rel = common_rel(Disc(disc_path)); defs = table_rows(rel, 0x111e04, 0x14a6c0, 8)
    cfg = world.common_table(rel, 0x80478F98); cur = struct.unpack(">h", rel[cfg + 4:cfg + 6])[0]; chain = []
    while 0 <= cur < len(defs) and len(chain) <= len(defs): chain.append(cur); cur = struct.unpack(">h", defs[cur][6:8])[0]
    block = savefile.current_block(open(save_path, "rb").read())
    return savefile.story_progress(block, defs, chain) if block is not None else None


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True); ap.add_argument("--disc", required=True); ap.add_argument("--work", required=True)
    ap.add_argument("--saferun", default=os.path.join(os.path.dirname(HERE), "saferun.py"))
    ap.add_argument("--reference", help="earlier run folder of the same route, to compare frame dumps")
    ap.add_argument("--save", help="a .gci the run wrote, for the story meter")
    ap.add_argument("routes", nargs="*")
    a = ap.parse_args()
    for r in a.routes or list(ROUTES):
        res = run(r, ROUTES[r], a.exe, a.disc, a.work, a.saferun)
        if a.reference: res["frame_diff"] = compare_frames(res["folder"], a.reference)
        if a.save: res["story"] = story_meter(a.save, a.disc)
        fb = os.path.join(res["folder"], "flags_end.bin")
        if os.path.exists(fb):   # the PC's flag dump: story progress of this run (flag_report.py's first line)
            out = subprocess.run([sys.executable, "-I", os.path.join(HERE, "flag_report.py"), fb, os.path.abspath(a.disc)],
                                 capture_output=True, text=True).stdout.splitlines()
            res["flags"] = out[0] if out else None
        print(("PASS " if res["ok"] else "FAIL ") + json.dumps(res, ensure_ascii=False))
