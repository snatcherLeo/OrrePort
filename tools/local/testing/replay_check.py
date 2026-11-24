"""One command for a sitting's PC replay check (2026-10-19; the project's ask for the S02 replay sync, snatcherLeo's top priority).

    python -I tools/local/testing/replay_check.py <sitting number or folder> [--exe <orrecolo.exe>] [--out <folder>]
        [--replay K] [--no-dvd] [--no-card] [--rng] [--time SECONDS] [--extra-env NAME=VALUE ...]

Chains the existing pieces:
1. a fresh scratch folder (default `_d_testing/replay_check/<sitting>_<time>`, outside the repo) with its own copy of
   the exe and the DLLs next to it (default: the play build, `pkmn_colosseum/build-pc/firstlink`);
2. the sitting's start card (`card_start/*.gci`) converted to the PC's byte order (`colo/savepc.py to-pc`) into the
   folder's `user/USA/Card A`;
3. the pad script from the sitting's movie with Dolphin's RNG state put back at every room entry
   (`dtm_to_pad.py --reseed`; needs ORRE_DEBUG=1);
4. from the sitting's Dolphin replays (the newest `replay_K` that has each file, or `--replay K`): the frame times
   (`frames.csv` -> `frames_to_pc.py` [-> `frames_steady.py` with --steady] -> ORRE_REPLAY_FRAMES), the disc reads (`dvd.csv` -> ORRE_DVD_TRACE) and the
   memory-card task states (`memcard.csv` -> ORRE_CARD_TRACE, read by the PC once B's card model lands);
5. the run itself through saferun (`--exclusive`, minimized window) with ORRE_SLICE_LOG / ORRE_BATTLE_LOG /
   ORRE_EVENT_LOG (and ORRE_RNG_TRACE with --rng). It stops on its own: a little after the PC has made as many room
   entries as the recording (the recording's tail after its last entry, plus 300 frames), 600 frames after the PC
   enters a room the recording didn't, when the log shows no new frame for 60 seconds (a stall), or when the pad script
   still has steps but none came for the next step's own wait plus 1200 frames (`--stuck-frames`; it names the step);
6. `replay_report.py` on the log: `report.md` next to `run.log`; the summary is printed.

`--from-room` (start at a room instead of power-on) comes once the PC side can start there (in progress).
Never run while snatcherLeo plays: saferun waits for him.
"""
import argparse
import datetime
import glob
import hashlib
import os
import queue
import re
import shutil
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
ROOT = os.path.abspath(os.path.join(REPO, ".."))
MAIN = os.path.join(ROOT, "pkmn_colosseum")
ISO = os.environ.get("PLAYREC_ISO") or os.path.join(MAIN, "orig", "Pokemon Colosseum (USA).iso")
DEFAULT_EXE = os.path.join(MAIN, "build-pc", "firstlink", "orrecolo.exe")
MINGW = r"C:\msys64\mingw64\bin"
sys.path.insert(0, HERE)
import playrec  # noqa: E402  (find_sitting, DEFAULT_ROOT)
import replay_report as rr  # noqa: E402

ENTER = re.compile(r"\[slice\] entered (0x[0-9A-Fa-f]+) frame=(\d+)")
ANYFRAME = re.compile(r"(?:frame[= ]|game frame )(\d+)")
STEP = re.compile(r"\[pad_script\] frame (\d+): step (\d+) ")
NSTEPS = re.compile(r"\[pad_script\] (\d+) steps from")
# an anchor arriving restarts the wait: "player appeared ... at frame F", "window 0xW ready at frame F", a room entry
ANCHOR = re.compile(r"\[pad_script\] (?:player appeared|window 0x[0-9A-Fa-f]+ ready) .*at frame (\d+)")
PADWAIT = re.compile(r"\+(\d+)")
FALLBACK = re.compile(r"~(\d+)")
STUCK_FRAMES = 1200
ROOMANCHOR = re.compile(r"@(0x[0-9A-Fa-f]+)")


def pad_steps(pad):
    """The pad script's steps in order (step k of the exe's log is entry k-1): the lines that aren't blank or comments."""
    with open(pad, encoding="utf-8") as f:
        return [l.strip() for l in f if l.strip() and not l.lstrip().startswith("#")]


def newest(sit, name, k=None):
    """The newest replay_K of the sitting that has `name` (or replay_K itself with --replay)."""
    if k is not None:
        p = os.path.join(sit, f"replay_{k}", name)
        return p if os.path.exists(p) else None
    reps = sorted(glob.glob(os.path.join(sit, "replay_*")), key=lambda p: int(p.rsplit("_", 1)[1]), reverse=True)
    return next((os.path.join(r, name) for r in reps if os.path.exists(os.path.join(r, name))), None)


def py(*args):
    subprocess.run([sys.executable, "-I", *args], check=True)


def place_binary(src, dst):
    """Hard-link dst to the shared copy of src's content (made once under _d_testing/replay_check/_bin/<sha1>/).
    The shared copies are never written again, so a rebuild can't change a run's binaries."""
    h = hashlib.sha1()
    with open(src, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    shared_dir = os.path.join(ROOT, "_d_testing", "replay_check", "_bin", h.hexdigest())
    shared = os.path.join(shared_dir, os.path.basename(dst))
    try:
        if not os.path.exists(shared):
            os.makedirs(shared_dir, exist_ok=True)
            shutil.copy2(src, shared + ".part")
            os.replace(shared + ".part", shared)
        os.link(shared, dst)
    except OSError:
        shutil.copy2(src, dst)


def prepare(a):
    sit = a.sitting if os.path.isdir(a.sitting) else playrec.find_sitting(playrec.DEFAULT_ROOT, a.sitting)
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    fadein_dir = None   # --fadein's replay folder, when used
    # absolute: the exe runs in `out` and reads its pad / frames files by these paths (a relative --out gave "cannot open
    # sitting.pad" and exit 3, 2026-10-20)
    out = os.path.abspath(a.out or os.path.join(ROOT, "_d_testing", "replay_check", f"{os.path.basename(sit)}_{stamp}"))
    os.makedirs(out)
    # 1. the exe and its DLLs (own copy: a relink in the build folder can't pull it away mid-run). Each run folder gets
    # hard links to one shared copy per file content (_bin/<sha1>/), not a fresh ~130 MB copy (2026-10-25: 448 runs
    # held 47 GB of identical binaries); a plain copy where linking fails
    exe_dir = os.path.dirname(os.path.abspath(a.exe))
    place_binary(a.exe, os.path.join(out, "orrecolo.exe"))
    # the exe's own folder first, then the play build's (an exe copied alone, as in a scratch folder, has no DLLs)
    for d in (exe_dir, os.path.dirname(exe_dir), os.path.dirname(DEFAULT_EXE), os.path.dirname(os.path.dirname(DEFAULT_EXE))):
        for dll in glob.glob(os.path.join(d, "*.dll")):
            if not os.path.exists(os.path.join(out, os.path.basename(dll))):
                place_binary(dll, os.path.join(out, os.path.basename(dll)))
    # 2. the start card
    gcis = glob.glob(os.path.join(sit, "card_start", "*.gci"))
    card = os.path.join(out, "user", "USA", "Card A")
    os.makedirs(card)
    for g in gcis:
        if a.card_order == "gc":
            # --card-order gc (2026-10-31): the console's card as-is. With 'pc' the exe accepts the converted
            # legacy slots and writes its own save in GameCube order, so every end card was mixed (BM1: slots 0-1 PC
            # order, slot 2 GameCube order), and the load path players use was never exercised
            shutil.copy2(g, os.path.join(card, os.path.basename(g)))
            continue
        py(os.path.join(REPO, "tools", "local", "colo", "savepc.py"), "to-pc", g, os.path.join(card, os.path.basename(g)))
    # 3. the pad script
    pad = os.path.join(out, "sitting.pad")
    if os.path.exists(os.path.join(sit, "sitting.dtm")):
        # the guide's replay recipe (Appendix C): stick from the latched input, presses timed from menu windows and
        # the player's reappearance, Dolphin's RNG at each room entry and battle menu, all from the probe replay
        rep = os.path.dirname(newest(sit, "input.csv", a.replay) or "")
        extra = []
        off = set(filter(None, (a.helpers_off or "").split(",")))  # --helpers-off (the match ledger's helper rows)
        if rep:
            extra += ["--poll", "second", "--latched", rep]
            if os.path.exists(os.path.join(rep, "windows.csv")):
                fa = newest(sit, "frames.csv", a.replay) if a.window_frameat else None
                if a.window_frameat and a.logic:
                    raise SystemExit("--window-frameat: not with --logic (its extra passes change the frame count)")
                if fa:
                    extra += ["--window-frameat", os.path.dirname(fa),
                              *([f"--press-frameat={a.press_frameat}"] if a.press_frameat else [])]
                if a.cursor_sync:
                    extra += [f"--cursor-sync={a.cursor_sync}"]
                extra += ["--windows", rep, *([f"--windows-rooms={a.windows_rooms}"] if a.windows_rooms else []),
                          *(["--windows-rooms-other"] if a.windows_rooms_other else []),
                          *(["--windows-rooms-unroomed"] if a.windows_rooms_unroomed else []),
                          *(["--fadein-hero-room"] if a.fadein_hero_room else []),
                          *([f"--fadein-return-rooms={a.fadein_return_rooms}"] if a.fadein_return_rooms else []),
                          *([f"--windows-rooms-lag={a.windows_rooms_lag}"] if a.windows_rooms_lag is not None else []),
                          *([f"--window-lag-rooms={a.window_lag_rooms}"] if a.window_lag_rooms else []),
                          *([f"--window-lag-ids={a.window_lag_ids}"] if a.window_lag_ids else []),
                          *([f"--press-lag={a.press_lag}"] if a.press_lag else [])]
                if os.path.exists(os.path.join(rep, "rng.csv")) and "reappear" not in off:
                    # --no-window-seed: no seeds in the battle windows, but still the one at each reappearance
                    shift = -1 if a.window_seed_shift is None else a.window_seed_shift  # the reappearances' calibration
                    extra += ["--window-seed", "--window-rng", rep, f"--window-seed-shift={shift}"]
                    if a.window_seed_shift is None:
                        extra += ["--no-battle-window-seed"]
                    if a.window_seed_rooms:
                        # the resync after a timing-only split (snatcherLeo's option 2026-10-25)
                        extra += [f"--window-seed-rooms={a.window_seed_rooms}"]
                        if a.window_seed_skip_ids:
                            extra += [f"--window-seed-skip-ids={a.window_seed_skip_ids}"]
                        if a.window_seed_only_ids:
                            extra += [f"--window-seed-only-ids={a.window_seed_only_ids}"]
                        if a.window_seed_burst:
                            extra += [f"--window-seed-burst={a.window_seed_burst}"]
                        if a.window_seed_keep:
                            extra += [f"--window-seed-keep={a.window_seed_keep}"]
                        own =newest(sit, "owners.csv") if a.window_seed_blink else None
                        if own:
                            # the blink timers with each seed (P08: a seed doesn't carry them)
                            extra += ["--window-seed-blink", os.path.dirname(own)]
            if os.path.exists(os.path.join(rep, "rng.csv")) and "burst" not in off:
                extra += ["--burst-seed", rep]  # the New Game burst (sitting 9's starters; dtm_to_pad.py)
                if "nameburst" in off:
                    # only the title screen's seed (the console's clock seed at boot), not the New Game burst's
                    extra += ["--burst-seed-max", "1"]
            if os.path.exists(os.path.join(rep, "pos.csv")):
                extra += ["--hero", rep, "--hero-rooms", f"--hero-lag={a.hero_lag}", "--battle-mash"]
        if a.fadein:
            # --fadein (2026-10-26; snatcherLeo's step 3): room presses timed from the fade-in's end, from the newest replay
            # with the fade probe (any replay of the sitting: they are label for label identical)
            fdir = os.path.dirname(newest(sit, "fade.csv") or "")
            if fdir:
                extra += ["--fadein", fdir, f"--fade-lag={a.fade_lag}"]
                fadein_dir = fdir
            else:
                print("[replay_check] --fadein: no replay of this sitting has fade.csv (playrec.py replay N --probe "
                      "fade); room-entry anchors kept")
        # reseed off: the same steps and press timing, without setting the random state (dtm_to_pad --reseed-noop)
        py(os.path.join(HERE, "dtm_to_pad.py"), sit, pad, "--reseed", *(["--reseed-noop"] if "reseed" in off else []),
           f"--entry-lag={a.entry_lag}", *([f"--entry-lag-rooms={a.entry_lag_rooms}"] if a.entry_lag_rooms else []),
           *([f"--entry-lag-every={a.entry_lag_every}"] if a.entry_lag_every else []),
           *([f"--entry-lag-nofade={a.entry_lag_nofade}"] if a.entry_lag_nofade else []),
           f"--window-lag={a.window_lag}", *extra)
    else:
        # no Dolphin movie (sitting 1): the per-frame pad log, one poll per frame, so not exact (playrec.py topc)
        py(os.path.join(HERE, "playrec.py"), "topc", os.path.basename(sit)[:3], pad, "--reseed")
    env = {"ORRE_WINDOW": "minimized", "ORRE_DEBUG": "1", "ORRE_SLICE_LOG": "1", "ORRE_BATTLE_LOG": "1",
           "ORRE_REPLAY_SPEED": "max",       # the replay clock runs as fast as the CPU allows (guide Appendix C)
           "ORRE_EVENT_LOG": "1", "ORRE_PAD_SCRIPT": "@file:" + pad.replace("\\", "/"),
           # a disc read the recording never saw takes at least a frame (pc_dvd.c, 2026-10-21): P02's Phenac
           # people started a frame or two early with the fast model and one wandered into Wes's path; with this the
           # replay gets past it (verified); P01 still reaches all its rooms. --extra-env overrides it
           "ORRE_DVD_UNRECORDED_MIN": "1",
           # the battle move camera's setups, for camera_compare.py against the Dolphin probe camparams (a row per
           # change; snatcherLeo, 2026-10-21: compare the battle cameras in every sitting)
           "ORRE_CAMPARAMS_LOG": "1",
           # the sound slots (last effect, music), for sound_compare.py against the Dolphin "sound" events
           "ORRE_SOUND_LOG": "1",
           # every sound start (battle sounds included), for sound_compare.py --sfx against the Dolphin sfx probe
           "ORRE_SFX_LOG": "1",
           "ORRE_RUN_WHY": f"replay_check: {os.path.basename(sit)} on the PC against its Dolphin recording"}
    used = {"sitting": sit, "card": gcis, "pad": pad}
    if fadein_dir:
        used["fadein"] = fadein_dir
    # 4. the recording's frame times, disc reads and card operations
    frames = newest(sit, "frames.csv", a.replay)
    if frames:
        ft = os.path.join(out, "sitting.frames")
        py(a.frames_tool, frames, ft)
        if a.steady:
            # the console's load hitches out, steady pacing kept (frames_steady.py)
            py(os.path.join(HERE, "frames_steady.py"), ft, ft + ".steady")
            ft += ".steady"
        elif a.logic and "logic" not in set(filter(None, (a.helpers_off or "").split(","))):
            # the console's logic passes at fades (2026-10-26; the project's candidate 2, option a): held and doubled passes
            # from the newest replay with the fade probe (frames_logic.py). Opt-in (--logic): alone it made P01 worse
            # (open gaps 60 -> 66 on the lane exe; it removes the fade-out's +1 that offset the gap's and fade-in's -1)
            fdir = os.path.dirname(newest(sit, "fade.csv") or "")
            if fdir and os.path.exists(os.path.join(os.path.dirname(frames), "replay_events.jsonl")):
                py(os.path.join(HERE, "frames_logic.py"), ft, os.path.dirname(frames), fdir, ft + ".logic")
                ft += ".logic"
                used["logic"] = fdir
        env["ORRE_REPLAY_FRAMES"] = "@file:" + ft.replace("\\", "/")
        used["frames"] = frames
        if a.rng_frames and os.path.exists(os.path.join(os.path.dirname(frames), "rng.csv")):
            # --rng-frames ROOMS|all (2026-10-25): the console's random state at every frame start in those rooms
            # (rngframes_to_pc.py from the same replay as the frame lengths, so the two files split alike)
            rf = os.path.join(out, "sitting.rngframes")
            py(os.path.join(HERE, "rngframes_to_pc.py"), os.path.dirname(frames), rf)
            env["ORRE_REPLAY_RNG"] = "@file:" + rf.replace("\\", "/")
            if a.rng_frames != "all":
                env["ORRE_REPLAY_RNG_ROOMS"] = a.rng_frames
            used["rngframes"] = rf
    dvd = None if a.no_dvd else newest(sit, "dvd.csv", a.replay)
    if dvd:
        env["ORRE_DVD_TRACE"] = dvd.replace("\\", "/")
        used["dvd"] = dvd
    # B's card model reads memcard.csv (the game's card task states), not card.csv (2026-10-19: card.csv parsed as
    # "0 busy stretches")
    cardcsv = None if a.no_card else newest(sit, "memcard.csv", a.replay)
    if cardcsv:
        env["ORRE_CARD_TRACE"] = cardcsv.replace("\\", "/")
        used["card_ops"] = cardcsv
    if a.rng:
        env["ORRE_RNG_TRACE"] = "1"
        used["rng_replay"] = os.path.dirname(newest(sit, "rng.csv", a.replay) or "") or None
    for kv in a.extra_env or []:
        k, _, v = kv.partition("=")
        env[k] = v
    with open(os.path.join(out, "check.env"), "w", encoding="utf-8") as f:
        for k, v in sorted(env.items()):
            f.write(f"{k}={v}\n")
        for k, v in used.items():
            f.write(f"# {k}: {v}\n")
        f.write(f"# stuck_frames: {a.stuck_frames}\n")
    return sit, out, used


def run_inner(out, sit):
    """Inside saferun: run the exe, copy its output to run.log, stop it when the recording's stretch is done."""
    env = dict(os.environ)
    for line in open(os.path.join(out, "check.env"), encoding="utf-8"):
        if line.strip() and not line.startswith("#"):
            k, _, v = line.rstrip("\n").partition("=")
            env[k] = v
    env["PATH"] = os.pathsep.join([out, MINGW, env.get("PATH", "")])
    stuck = STUCK_FRAMES
    for line in open(os.path.join(out, "check.env"), encoding="utf-8"):
        if line.startswith("# stuck_frames: "):
            stuck = int(line.split(":", 1)[1])
    steps = pad_steps(env["ORRE_PAD_SCRIPT"].removeprefix("@file:")) if env.get("ORRE_PAD_SCRIPT") else []
    de = rr.load_dolphin(sit)[0]
    # the room order to stop on: entered plus the "reentered" reloads the PC also reports (rr.recording_entries,
    # 2026-10-29: P09's return to 0x1D after battle 158)
    rec_rooms = [e["room"] for e in rr.recording_entries(sit)]
    tail = (rr.load_dolphin(sit)[3] - de[-1]["f"]) if de else 0
    log = open(os.path.join(out, "run.log"), "w", encoding="utf-8", buffering=1)
    p = subprocess.Popen([os.path.join(out, "orrecolo.exe"), "--disc", ISO], cwd=out, env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8",
                         errors="replace", bufsize=1)
    lines = queue.Queue()
    # a reader thread: a pipe read blocks on Windows, and the stall check must still run when the exe goes quiet
    threading.Thread(target=lambda: [lines.put(l) for l in p.stdout] and None or lines.put(None), daemon=True).start()
    entries, stop_at, why = [], None, "the exe exited"
    cur, last_progress = 0, time.time()
    # the stuck-script stopper (2026-10-20: P02's replay stood ~44,000 frames in Phenac's square waiting for the
    # Fun Old Man's battle room, frames still moving, so neither stopper above fired): with steps left, no step for the
    # next step's own wait plus `stuck` frames since the last step or anchor stops the run and names what it waited for
    nsteps, last_step, since, entry_frames = 0, 0, None, []
    while True:
        try:
            chunk = lines.get(timeout=1)
        except queue.Empty:
            chunk = ""
        if chunk is None:
            break
        if chunk:
            log.write(chunk)
            m = ENTER.search(chunk)
            if m:
                room, f = int(m.group(1), 16), int(m.group(2))
                k = len(entries)
                entries.append(room)
                entry_frames.append(f)
                if k >= len(rec_rooms) - 1 and stop_at is None:
                    stop_at, why = f + tail + 300, f"the recording's {len(rec_rooms)} room entries reached"
                if k < len(rec_rooms) and room != rec_rooms[k] and stop_at is None:
                    stop_at, why = f + 600, f"room entry {k + 1} is {room:#x}, the recording's is {rec_rooms[k]:#x}"
            for m in ANYFRAME.finditer(chunk):
                n = int(m.group(1))
                if n > cur:
                    cur, last_progress = n, time.time()
            m = NSTEPS.search(chunk)
            if m:
                nsteps = int(m.group(1))
            m = STEP.search(chunk)
            if m:
                since, last_step = int(m.group(1)), int(m.group(2))
            m = ANCHOR.search(chunk) or ENTER.search(chunk)
            if m and since is not None:
                since = max(since, int(m.group(m.lastindex)))
            if since is not None and 0 < last_step < nsteps and last_step < len(steps):
                nxt = steps[last_step]   # step last_step + 1
                w = PADWAIT.search(nxt.split(":", 1)[0])
                limit = since + (int(w.group(1)) if w else 0) + stuck
                g = FALLBACK.search(nxt.split(":", 1)[0])
                if g:
                    # an anchor with a fallback ("@X+N~G") fires at the latest G frames after the previous step, so the
                    # run isn't stuck before then (2026-10-21: the reappearance after a battle waits up to 3,600)
                    limit = max(limit, since + int(g.group(1)) + stuck)
                r = ROOMANCHOR.match(nxt)
                k = len(entries)
                if r and int(r.group(1), 16) < 0x10000 and 0 < k < len(de):
                    # a room entry: cutscenes run long (P01's walk to Outskirt Stand, ~3,900 frames), so the limit is
                    # the recording's own gap between this entry and the next, from the PC's last entry
                    limit = max(limit, entry_frames[-1] + de[k]["f"] - de[k - 1]["f"] + stuck)
                if cur > limit:
                    why = (f"stuck: no pad step for {cur - since} frames after step {last_step} of {nsteps}; step "
                           f"{last_step + 1} waits for '{nxt}'")
                    break
            if stop_at is not None and cur >= stop_at:
                break
        if time.time() - last_progress > 60:
            why = f"stall: no new frame for 60 s (last frame {cur})"
            break
    if p.poll() is None:
        p.kill()
        p.wait()
    log.write(f"[replay_check] stopped at frame {cur}: {why}\n")
    log.close()
    print(f"[replay_check] stopped at frame {cur}: {why}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sitting", help="sitting number (playrec.py) or a sitting folder")
    ap.add_argument("--exe", default=DEFAULT_EXE)
    ap.add_argument("--out")
    ap.add_argument("--card-order", choices=("pc", "gc"), default="gc",
                    help="the start card's byte order: 'pc' converts each slot with savepc.py to-pc (the PC's old "
                         "order, from before memcard.c switched to GameCube order on 2026-10-18), 'gc' copies the "
                         "sitting's card as-is, the real player path through pc_save_swap_fields (2026-10-31). "
                         "Default 'gc' since 2026-10-31 evening (the OK): P24 and P25 compare the same with it, and "
                         "sitting 43 saves with it (after port-gaps 46)")
    ap.add_argument("--replay", type=int, help="take frames / dvd / card captures from this replay_K only")
    ap.add_argument("--no-dvd", action="store_true", help="instant disc reads (no ORRE_DVD_TRACE)")
    ap.add_argument("--no-card", action="store_true", help="no ORRE_CARD_TRACE")
    ap.add_argument("--logic", action="store_true",
                    help="the console's held / extra logic passes at fades in the frame times (frames_logic.py; "
                         "opt-in: alone it made P01 worse, see gamecube-accuracy.md \"Logic passes at fade-ins\")")
    ap.add_argument("--steady", action="store_true",
                    help="frame times without the console's single long frames (load hitches; frames_steady.py)")
    ap.add_argument("--rng", action="store_true", help="ORRE_RNG_TRACE=1 and the report's per-frame RNG comparison")
    ap.add_argument("--time", type=int, default=1800, help="saferun's time cap in seconds")
    ap.add_argument("--extra-env", nargs="*", help="more NAME=VALUE settings for the run")
    # --fadein on by default since 2026-10-26 (the project's OK after the A/B on P01, P02, P05, P08, P10, P11: nothing lost,
    # P08 and P11 unstuck, P10 closer; battle rooms keep their window / reappearance timing; lag 1 as --hero-lag)
    ap.add_argument("--fadein", action=argparse.BooleanOptionalAction, default=True,
                    help="dtm_to_pad.py --fadein: time each room's presses from its fade-in's end (needs a replay with "
                         "the fade probe; --no-fadein for the room-entry timing)")
    ap.add_argument("--fade-lag", type=int, default=1, help="dtm_to_pad.py --fade-lag (1: calibrated on P01)")
    ap.add_argument("--hero-lag", type=int, default=1,
                    help="dtm_to_pad.py --hero-lag: retraces added to presses timed from the player's appearance (1, "
                         "calibrated in 60 fps rooms; sitting 9's 30 fps Phenac talk is the test for other values)")
    # Measured on P01 with only the title's clock seed (2026-10-21): +1 on the name screen (0x390) puts every room entry
    # through Willie's battle room on the console's random state and rolls the console's starters; 0x384 and 0x3A1 are
    # indifferent; 0x38F needs 0 (with +1 the random state parts at the next room). Why the rooms differ isn't known yet
    ap.add_argument("--entry-lag", type=int, default=1,
                    help="dtm_to_pad.py --entry-lag: frames added to the first press after a room entry, in "
                         "--entry-lag-rooms only (default 1)")
    ap.add_argument("--entry-lag-rooms", default="0x390",
                    help="dtm_to_pad.py --entry-lag-rooms: the rooms (hex, comma list; default 0x390, the name screen)")
    ap.add_argument("--entry-lag-nofade", type=int, default=0,
                    help="dtm_to_pad.py --entry-lag-nofade: frames added to the first press of every non-battle "
                         "visit with no fade-in anchor (2 lines the room changes up: P02 48 -> 41 open gaps). Off by "
                         "default (2026-10-26, reversed the same evening): on P01 it cost exact state, 2,320 "
                         "draws against the console's 2,309 through the world map, so for gap reviews only")
    ap.add_argument("--entry-lag-every", default="",
                    help="dtm_to_pad.py --entry-lag-every: rooms whose every visit gets --entry-lag (hex, comma list)")
    ap.add_argument("--windows-rooms", default="",
                    help="dtm_to_pad.py --windows-rooms: room visits whose presses count from window openings "
                         "(hex room, ':N' for its Nth visit; comma list)")
    ap.add_argument("--window-seed-rooms", default="",
                    help="dtm_to_pad.py --window-seed-rooms: resync at a timing-only split, Dolphin's random state at "
                         "each battle menu of these room visits (ROOM[:VISIT]@FROM[-TO][~SHIFT],...)")
    ap.add_argument("--window-seed-skip-ids", default="",
                    help="dtm_to_pad.py --window-seed-skip-ids: window ids (hex) that never get a resync seed (0x50, "
                         "the battle message box, opens inside move animations)")
    ap.add_argument("--rng-frames", default="",
                    help="ROOMS (hex, comma list) or 'all': set the PC's random state to the console's at every frame "
                         "start in those rooms (rngframes_to_pc.py, ORRE_REPLAY_RNG; a bug-hunting mode: rolls then "
                         "match, so what still differs is behaviour)")
    ap.add_argument("--window-seed-burst", type=int, default=0,
                    help="dtm_to_pad.py --window-seed-burst: a seed whose recorded frame drew more than N numbers takes "
                         "the state from before that frame")
    ap.add_argument("--window-seed-keep", type=int, default=0,
                    help="dtm_to_pad.py --window-seed-keep: each resync seed lists Dolphin's states N frames around it, "
                         "and the PC keeps its own state when it is one of them (2026-10-26, P12)")
    ap.add_argument("--window-frameat", action="store_true",
                    help="dtm_to_pad.py --window-frameat with the replay the frame times come from: at each anchored "
                         "window opening the PC's recorded frame lengths jump to the console's frame there ("
                         "2026-10-28, P07's box visit; not with --logic, whose extra passes change the frame count)")
    ap.add_argument("--cursor-sync", metavar="ROOMS", default="",
                    help="dtm_to_pad.py --cursor-sync ROOMS: before each A press in these menu rooms the PC's open "
                         "menu cursors are set to the console's (a replay resync, 2026-10-29; 0x391,0x38F)")
    ap.add_argument("--press-frameat", metavar="ROOMS", default="",
                    help="with --window-frameat: dtm_to_pad.py --press-frameat ROOMS, the frame lengths also "
                         "re-anchored at every press in these menu rooms (timed from the entry; 0x391, the PC box)")
    ap.add_argument("--window-seed-blink", action="store_true",
                    help="dtm_to_pad.py --window-seed-blink with the sitting's newest owners.csv: each resync seed also "
                         "sets the battle's idle blink timers to the console's")
    ap.add_argument("--window-seed-only-ids", default="",
                    help="dtm_to_pad.py --window-seed-only-ids: resync seeds only where these windows open (0x47, the "
                         "command menu)")
    ap.add_argument("--window-lag-rooms", default="",
                    help="dtm_to_pad.py --window-lag-rooms: per-room --window-lag for battle-window presses (ROOM:LAG,...)")
    ap.add_argument("--window-lag-ids", default="",
                    help="dtm_to_pad.py --window-lag-ids: a press lag by window kind (ID:LAG,..., hex ids)")
    ap.add_argument("--press-lag", default="",
                    help="dtm_to_pad.py --press-lag: one press later, the rest on time (ROOM:VISIT@FRAME:LAG,...)")
    ap.add_argument("--windows-rooms-other", action="store_true",
                    help="dtm_to_pad.py --windows-rooms-other: those visits' other windows (a cutscene's) instead")
    ap.add_argument("--windows-rooms-unroomed", action="store_true",
                    help="dtm_to_pad.py --windows-rooms-unroomed: also the room -1 windows inside those visits")
    ap.add_argument("--fadein-hero-room", action="store_true",
                    help="dtm_to_pad.py --fadein-hero-room: a fade-in after a battle's reappearance takes its room")
    ap.add_argument("--fadein-return-rooms", default="",
                    help="dtm_to_pad.py --fadein-return-rooms: the field's fade-in after these menu rooms is an anchor")
    ap.add_argument("--windows-rooms-lag", type=int, default=None,
                    help="dtm_to_pad.py --windows-rooms-lag: frames added to presses timed from those windows")
    ap.add_argument("--window-lag", type=int, default=0,
                    help="dtm_to_pad.py --window-lag: frames added to a press timed from a menu window's opening")
    ap.add_argument("--stuck-frames", type=int, default=STUCK_FRAMES,
                    help="stop when no pad step comes for the next step's own wait plus this many frames")
    # the main checkout's frames_to_pc.py when it has one (the project's newer version splits at the replay's entry events)
    main_ft = os.path.join(MAIN, "tools", "local", "testing", "frames_to_pc.py")
    ap.add_argument("--frames-tool", default=main_ft if os.path.exists(main_ft) else os.path.join(HERE, "frames_to_pc.py"))
    ap.add_argument("--prepare-only", action="store_true", help="build the folder and its inputs, don't run")
    # 2026-10-21: with the disc reads timed exactly (ORRE_DVD_TRACE_SHIFT=1), P01's Willie battle is draw-for-draw
    # Dolphin's up to the first battle box, where the seed taken one frame early (-1) put the PC back 8 draws (the
    # camera's own draws in the opening frame) and the blinks and camera picks drifted from there
    # Default since 2026-10-21 (with pc_arq.c's ARAM timing): no seeds in the battle windows; P01 then reaches all 16
    # rooms with 14 and 18 identical move-camera setups in its two battles (verified). The reappearance seeds stay
    ap.add_argument("--window-seed-shift", type=int, default=None,
                    help="seed at every battle window opening too, from rng.csv this many frames after it (the old "
                         "default was -1)")
    ap.add_argument("--helpers-off", metavar="LIST",
                    help="replay helpers to leave out, comma-separated: reseed (Dolphin's random state at every room "
                         "entry), burst (before the New Game burst), reappear (after each battle), logic (the "
                         "console's held / doubled logic passes at fades, frames_logic.py); what still matches "
                         "without them is the port's own accuracy (2026-10-20, the match ledger)")
    ap.add_argument("--no-window-seed", dest="window_seed_shift", action="store_const", const=None,
                    help="no RNG seeds at battle window openings (the default)")
    ap.add_argument("--inner", nargs=2, help=argparse.SUPPRESS)   # (out, sitting): the run inside saferun
    a = ap.parse_args()
    if a.inner:
        run_inner(*a.inner)
        return
    sit, out, used = prepare(a)
    print(f"[replay_check] {out}")
    for k, v in used.items():
        print(f"  {k}: {v}")
    if a.prepare_only:
        print("[replay_check] prepared only; check.env holds the run's settings")
        return
    subprocess.run([sys.executable, "-I", os.path.join(REPO, "tools", "local", "saferun.py"), "--exclusive",
                    "--time", str(a.time), "--why", f"D replay_check: {os.path.basename(sit)} PC replay vs its Dolphin "
                    "recording (S02 replay sync)", "--", sys.executable, "-I", os.path.abspath(__file__), a.sitting,
                    "--inner", out, sit])
    rep = [sys.executable, "-I", os.path.join(HERE, "replay_report.py"), sit, os.path.join(out, "run.log"),
           "--out", os.path.join(out, "report.md")]
    if a.rng and used.get("rng_replay"):
        rep += ["--dolphin-replay", used["rng_replay"]]
    # the child's stdout through a pipe is in the console code page on Windows, not UTF-8: decode with errors="replace"
    # (a "±" in the report made the reader thread fail and leave stdout None, 2026-10-26); report.md itself is UTF-8
    text = subprocess.run(rep, capture_output=True, text=True, encoding="utf-8", errors="replace").stdout or ""
    print(text[text.find("## Summary"):] if "## Summary" in text else text)
    print(f"[replay_check] report: {os.path.join(out, 'report.md')}")


if __name__ == "__main__":
    main()
