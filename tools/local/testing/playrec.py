"""The playthrough recorder: one command per sitting (2026-10-18; the project notes section 0, snatcherLeo's playthrough plan).

Everything lives outside the repo, under ROOT (default: the `_playthrough` folder next to the repo; never committed):

    ROOT/dolphin-user/          snatcherLeo's own Dolphin settings folder (a copy of the scripting fork's, made by `setup`;
                                the folder stays untouched). His controller mapping and graphics choices live here.
    ROOT/sittings/index.jsonl   one line per sitting (number, start, end, frames, marks, rooms, where it started)
    ROOT/sittings/NNN_<date>/   one folder per sitting:
        settings/               the Dolphin .ini files as they were at the start (replays use exactly these)
        card_start/ card_end/   the memory card before and after
        pad.jsonl               every change of controller 1 as the game read it (frame, 10 PADStatus bytes)
        events.jsonl            what the game did (dolphin_playrec.py lists the events)
        ids.jsonl               rooms and battles (idlog.py), flags_entry_*.json the flag buffers at each entry
        states/                 save states: first entry of each room, battle starts, every 5 minutes, marks, end
        blocks/                 the live save block at every room entry (zlib)
        sitting.json            last frame, marks, how it ended
        replay_N/               each replay: replay_events.jsonl, replay_check.jsonl (input mismatches)

Commands (python -I tools/local/testing/playrec.py ...):
    setup                       make ROOT/dolphin-user from the fork's folder (once)
    settings                    open Dolphin on ROOT/dolphin-user (controller, graphics; no game, nothing recorded)
    start --section SNN [--note "..."] [--state FILE]
                                a new sitting of section SNN (S01-S16, BM, TEST): power-on with the card (default) or from
                                a save state; filed under ROOT/sections/SNN_<name>/
    --game xd ...               XD's sittings (2026-11-24, snatcherLeo's XD Dolphin-recordings exception): setup, settings,
                                start --section XD and replay N only, under _playthrough_xd, with dolphin_xdrec.py
                                (no game addresses; a replay is checked by RAM hashes against the earlier replays)
    replay N [--speed 0]        replay sitting N unthrottled in a scratch copy of its settings and card, then check
    check N [--replay K]        compare the recording with a replay: inputs and the event log, first difference
    report N                    the sitting report: time, rooms, battles, flags, marks, moves used
    topc N OUT.pad              the sitting's presses as a PC pad script (ORRE_PAD_SCRIPT=@file:OUT.pad)

Replay-critical Dolphin settings (checked at start; a change is reported, never fixed silently): Dual Core off
(CPUThread = False), Custom RTC on with a fixed value (the game seeds its RNG from the clock), the CPU engine and the
DSP (HLE / LLE) unchanged across the playthrough.
"""
import argparse, datetime, glob, hashlib, json, os, re, shutil, struct, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
DOLPHIN_DIR = os.path.abspath(os.path.join(REPO, "..", "_external", "dolphin-scripting", "app"))
DOLPHIN = os.path.join(DOLPHIN_DIR, "Dolphin.exe")
WATCHDOG_NAMES = ["Dolphin.exe"]        # what run_dolphin's watchdog may end by folder (a test swaps it)
FORK_USER = os.path.join(DOLPHIN_DIR, "User")
# PLAYREC_ISO: the disc image when this checkout has none (a lane worktree, 2026-10-19)
ISO = os.environ.get("PLAYREC_ISO") or os.path.join(REPO, "orig", "Pokemon Colosseum (USA).iso")
DEFAULT_ROOT = os.path.abspath(os.path.join(REPO, "..", "_playthrough"))
CARD_REL = os.path.join("GC", "USA", "Card A")
# XD (2026-11-24; snatcherLeo's XD Dolphin-recordings exception in the project rules: XD sittings recorded in Dolphin as the
# future XD port's reference, no XD decomp and no XD code in the port). `--game xd` switches the disc, the root folder
# (_playthrough_xd, so XD's card, states and sittings never mix with Colosseum's) and the Dolphin-side script
# (dolphin_xdrec.py, which reads no game address). Everything else (the pinned fork, the settings check, the movie
# collection, the watchdog) is the same code. Set in main().
GAME = "colo"
XD_ISO = os.environ.get("PLAYREC_XD_ISO") or os.path.join(REPO, "orig", "Pokemon XD - Gale of Darkness (USA).iso")
XD_ROOT = os.path.abspath(os.path.join(REPO, "..", "_playthrough_xd"))
GAME_IDS = {"colo": "GC6E01", "xd": "GXXE01"}
SKIP_ON_COPY = {"StateSaves", "Cache", "Dump", "Logs", "ScreenShots", "Shaders", "Load", "ResourcePacks"}
CRITICAL = [("Core", "CPUThread"), ("Core", "EnableCustomRTC"), ("Core", "CustomRTCValue"), ("Core", "CPUCore"),
            ("DSP", "EnableJIT"), ("Core", "DSPHLE"), ("Core", "SyncGPU"), ("Core", "MMU"), ("Core", "FastDiscSpeed")]


def ini(path):
    out, sec = {}, ""
    if not os.path.exists(path):
        return out
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.strip()
        if line.startswith("[") and line.endswith("]"):
            sec = line[1:-1]
        elif "=" in line:
            k, v = line.split("=", 1)
            out[(sec, k.strip())] = v.strip()
    return out


def critical(config_dir):
    d = ini(os.path.join(config_dir, "Dolphin.ini"))
    return {f"{s}.{k}": d.get((s, k), "(default)") for s, k in CRITICAL}


def game_ini_overrides(user):
    """The replay-critical settings a game's own INI files set over Dolphin.ini (Dolphin applies Sys/GameSettings/<GXX>
    .ini, then <GXXE01>.ini, then the user folder's GameSettings/<GXXE01>.ini). Why: XD's built-in GXX.ini turns MMU on
    and Dual Core off for the game (verified 2026-11-24 in the pinned build's Sys folder; Colosseum's GC6.ini sets
    neither), so Dolphin.ini alone doesn't say what a replay runs with. Recorded in each sitting's meta.json."""
    gid = GAME_IDS[GAME]
    files = [os.path.join(DOLPHIN_DIR, "Sys", "GameSettings", gid[:3] + ".ini"),
             os.path.join(DOLPHIN_DIR, "Sys", "GameSettings", gid + ".ini"),
             os.path.join(user, "GameSettings", gid + ".ini")]
    out = {}
    for f in files:
        d = ini(f)
        for s, k in CRITICAL:
            if (s, k) in d:
                out[f"{s}.{k}"] = f"{d[(s, k)]} ({os.path.basename(os.path.dirname(os.path.dirname(f)))}/{os.path.basename(f)})"
    return out


def copy_user(src, dst):
    os.makedirs(dst, exist_ok=True)
    for name in os.listdir(src):
        if name in SKIP_ON_COPY:
            continue
        s, t = os.path.join(src, name), os.path.join(dst, name)
        if os.path.isdir(s):
            shutil.copytree(s, t, dirs_exist_ok=True)
        else:
            shutil.copy2(s, t)


BM_USER = "dolphin-user-bm"   # battle mode's own profile and card (save edit E-BM, guide part 9; snatcherLeo, 2026-10-20)


def cmd_setup_bm(a):
    """The battle-mode profile: a copy of the recording profile (settings, controller, Hotkeys.ini) with an EMPTY
    card, so battle-mode teams go on a copy of a story save and his story card is never touched. Main puts the E-BM
    save on its card. Refuses if the profile exists."""
    src = a.user or os.path.join(a.root, "dolphin-user")
    dst = os.path.join(a.root, BM_USER)
    if os.path.exists(dst):
        sys.exit(f"{dst} exists already; nothing changed")
    if not os.path.isdir(src):
        sys.exit(f"no recording profile at {src}")
    os.makedirs(dst)
    for name in os.listdir(src):
        if name in SKIP_ON_COPY:
            continue
        s, t = os.path.join(src, name), os.path.join(dst, name)
        if os.path.isdir(s):
            # the card folder is created but nothing in it is copied: his saves stay only on his card
            shutil.copytree(s, t, ignore=lambda d, names: names if os.path.basename(d) == "Card A" else [])
        else:
            shutil.copy2(s, t)
    print(f"made {dst} (card empty: {os.path.join(dst, CARD_REL)})")


def sittings_dir(root):
    d = os.path.join(root, "sittings")
    os.makedirs(d, exist_ok=True)
    return d


# The playthrough's sections (snatcherLeo, 2026-10-18; docs/testing/playthrough-guide.md section 5): each sitting is filed
# under the section it plays, ROOT/sections/<id>_<name>/NNN_<date>_<time>/ (numbers stay global across sections).
SECTIONS = {
    "S01": "outskirt_to_phenac", "S02": "phenac", "S03": "pyrite_1", "S04": "pyrite_colosseum_building",
    "S05": "pyrite_cave_miror_b", "S06": "pyrite_side", "S07": "agate_relic", "S08": "mt_battle",
    "S09": "agate_after", "S10": "the_under", "S11": "subway_shadow_liner", "S12": "shadow_lab",
    "S13": "realgam_domes", "S14": "realgam_to_end", "S15": "postgame", "S16": "gba_bonus_ereader",
    "BM": "battle_mode",   # battle mode's battles, no story (snatcherLeo, 2026-10-19; playthrough guide v2 part 8, BM1-BM5)
    "FREE": "free",        # no plan code: snatcherLeo's check-out words say what the sitting did (snatcherLeo, 2026-10-24)
    "TEST": "tests",
    "XD": "xd",            # XD's sittings (`--game xd`, under _playthrough_xd; 2026-11-24)
}


def section_dir(root, sec):
    sec = sec.upper()
    if sec not in SECTIONS:
        sys.exit(f"unknown section {sec}: one of {', '.join(SECTIONS)}")
    d = os.path.join(root, "sections", f"{sec}_{SECTIONS[sec]}")
    os.makedirs(d, exist_ok=True)
    return d


def all_sittings(root):
    """(n, folder) for every sitting, in the old flat folder and in the section folders."""
    out = []
    flat = os.path.join(root, "sittings")
    dirs = [flat] if os.path.isdir(flat) else []
    secs = os.path.join(root, "sections")
    if os.path.isdir(secs):
        dirs += [os.path.join(secs, s) for s in sorted(os.listdir(secs))]
    for d in dirs:
        for name in os.listdir(d):
            p = os.path.join(d, name)
            if name[:3].isdigit() and os.path.isdir(p):
                out.append((int(name[:3]), p))
    return sorted(out)


def find_sitting(root, n):
    if str(n).lower() == "latest":
        # "latest" (snatcherLeo, 2026-10-21: "report latest", so he needn't remember the number): the highest-numbered sitting
        found = max(all_sittings(root), key=lambda kp: kp[0], default=None)
        if found is None:
            sys.exit(f"no sitting under {root}")
        print(f"(latest: sitting {found[0]}, {os.path.basename(found[1])})")
        return found[1]
    for k, p in all_sittings(root):
        if k == int(n):
            return p
    sys.exit(f"no sitting {n} under {root}")


def log_windows(pid, out, tag):
    """Append the state of every visible top-level window of process `pid` to <out>/window_state.txt (2026-11-06,
    Main: prove PLAYREC_WINDOW=minimized holds without asking snatcherLeo): title, minimized (IsIconic), foreground, rect."""
    if os.name != "nt":
        return
    import ctypes
    from ctypes import wintypes
    u = ctypes.windll.user32
    fg = u.GetForegroundWindow()
    rows = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def each(h, _):
        p = wintypes.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and u.IsWindowVisible(h):
            t = ctypes.create_unicode_buffer(200)
            u.GetWindowTextW(h, t, 200)
            r = wintypes.RECT()
            u.GetWindowRect(h, ctypes.byref(r))
            rows.append("  %r minimized=%d foreground=%d rect=%d,%d,%d,%d" % (
                t.value, bool(u.IsIconic(h)), h == fg, r.left, r.top, r.right, r.bottom))
        return True
    try:
        u.EnumWindows(each, 0)
    except Exception as e:                  # (a failed query must never stop the run)
        rows.append("  query failed: %s" % e)
    with open(os.path.join(out, "window_state.txt"), "a") as f:
        f.write("%s %s (pid %d): %d visible window(s)\n%s\n" % (
            datetime.datetime.now().isoformat(timespec="seconds"), tag, pid, len(rows), "\n".join(rows)))


def run_dolphin(user, out, mode, state="", pad="", speed=None, extra_env=None, batch=True, movie=None):
    env = dict(os.environ, PLAYREC_OUT=out, PLAYREC_MODE=mode, PLAYREC_STATE=state, PLAYREC_PAD=pad,
               TESTING_DIR=HERE)
    env.update(extra_env or {})
    env["PLAYREC_GAME"] = GAME
    script = "dolphin_xdrec.py" if GAME == "xd" else "dolphin_playrec.py"
    cmd = [DOLPHIN, "-u", user] + (["-b"] if batch else []) + ["-e", ISO] + (["-m", movie] if movie else []) \
        + ["--script", os.path.join(HERE, script)]
    if speed is not None:
        cmd[1:1] = ["-C", f"Dolphin.Core.EmulationSpeed={speed}"]
    if mode != "record" and env.get("PLAYREC_DUMPAUDIO") == "1":
        # PLAYREC_DUMPAUDIO=1 (2026-11-07; P12 notes 7 and 8, the console's sound for clips): Dolphin's audio dump
        # for this run only (a -C override, nothing written to any settings folder): <user>/Dump/Audio/dspdump.wav
        # (the DSP mixer, all game sound) and dtkdump.wav (disc streaming), from boot to the end of the run, in
        # emulated time, so a clip is cut by frame (audio_clip.py). Never in record mode
        cmd[1:1] = ["-C", "Dolphin.DSP.DumpAudio=True", "-C", "Dolphin.DSP.DumpAudioSilent=True"]
    if not os.path.exists(ISO):
        # (2026-11-05: without the disc Dolphin quits at once with no log, and the run looks finished; it happened
        # three times in one evening from a worktree with no orig/. Set PLAYREC_ISO to the main checkout's disc)
        sys.exit("no disc at %s: set PLAYREC_ISO (a worktree has no orig/)" % ISO)
    print("running:", " ".join(cmd), flush=True)
    # the no-progress watchdog (2026-11-05; snatcherLeo's rule that a stuck run ends itself, with the reason logged):
    # dolphin_playrec.py writes <out>/heartbeat.txt every 300 frames; with no heartbeat PLAYREC_WATCHDOG_SECS (default
    # 240; 0 = off) after the launch, or none newer for that long, Dolphin is ended and the reason goes to
    # <out>/watchdog.txt. Never in record mode (snatcherLeo plays, and may pause).
    limit = int(env.get("PLAYREC_WATCHDOG_SECS", "240"))
    # Minimized by default (2026-11-06; snatcherLeo saw a replay window pop up while at the PC): Windows starts Dolphin
    # minimized and without focus (STARTUPINFO, SW_SHOWMINNOACTIVE); PLAYREC_WINDOW=normal shows it. Never in record
    # mode (snatcherLeo plays there). Proven 2026-11-06 (028 replay_6): window_state.txt showed the one window minimized and
    # not in front, and the shots rendered normally (the project notes).
    si = None
    if os.name == "nt" and mode != "record" and env.get("PLAYREC_WINDOW", "minimized") == "minimized":
        si = subprocess.STARTUPINFO()
        si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        si.wShowWindow = 7    # SW_SHOWMINNOACTIVE
    proc = subprocess.Popen(cmd, env=env, cwd=DOLPHIN_DIR, startupinfo=si)
    if mode == "record" or limit <= 0:
        return proc.wait()
    hb, t0 = os.path.join(out, "heartbeat.txt"), time.time()
    if os.path.exists(hb):
        os.remove(hb)                       # (a stale heartbeat from an earlier run in the same folder)
    checks = [10, 60, 300]                  # seconds after the launch: log the window state (log_windows)
    while proc.poll() is None:
        time.sleep(5)
        if checks and time.time() - t0 >= checks[0]:
            log_windows(proc.pid, out, "%d s after launch" % checks.pop(0))
        last = os.path.getmtime(hb) if os.path.exists(hb) else None
        idle = time.time() - (last or t0)
        if idle > limit:
            try:
                frame = open(hb).read().split()[0]
            except Exception:
                frame = None
            why = ("no heartbeat %d s after the launch: the Dolphin script never ran its frame hook" % idle
                   if last is None else "no new heartbeat for %d s (last at frame %s): the script or the emulator "
                   "stopped advancing" % (idle, frame))
            print("[watchdog] ending Dolphin: " + why, flush=True)
            with open(os.path.join(out, "watchdog.txt"), "a") as f:
                f.write("%s %s\n" % (datetime.datetime.now().isoformat(timespec="seconds"), why))
            # (no captured output and a time limit on each kill: the watchdog itself must never hang; captured pipes
            # did hang in its test, and so did taskkill /T, so the launched process is ended directly)
            quiet = dict(stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60)
            try:
                proc.kill()
            except OSError:
                pass
            # and anything left whose command line names this run's folder (the real program can run outside the
            # launched process's tree: saferun has met a Dolphin.exe "outside the job", and the Store python's stub
            # did the same in the watchdog's test). Only processes named in WATCHDOG_NAMES: a shell or monitor whose own
            # command line names the folder must survive (a first version killed its own PowerShell that way)
            ps = ("Get-CimInstance Win32_Process | Where-Object { @(%s) -contains $_.Name -and $_.CommandLine -and "
                  "$_.CommandLine.Contains('%s') } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }"
                  % (",".join("'%s'" % n for n in WATCHDOG_NAMES), out.replace("'", "''")))
            try:
                subprocess.run(["powershell", "-NoProfile", "-Command", ps], **quiet)
            except subprocess.TimeoutExpired:
                pass
            try:
                proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                print("[watchdog] the launched process didn't exit 30 s after the kill", flush=True)
            return 0x57                     # (a distinct exit code: the watchdog ended it)
    return proc.returncode


def cmd_setup(a):
    user = os.path.join(a.root, "dolphin-user")
    if os.path.exists(user):
        sys.exit(f"{user} exists already; nothing changed")
    src = FORK_USER
    colo_user = os.path.join(DEFAULT_ROOT, "dolphin-user")
    if GAME == "xd" and os.path.isdir(colo_user):
        # XD's profile starts as a copy of snatcherLeo's Colosseum recording profile (his controller mapping, hotkeys and the
        # same replay-critical settings), with the card emptied below; Colosseum's profile is only read
        src = colo_user
    copy_user(src, user)
    if src != FORK_USER:
        print(f"copied the settings from {src} (the card is emptied, Colosseum's saves stay on its own card)")
    card = os.path.join(user, CARD_REL)
    if os.path.isdir(card) and not a.keep_card:
        for f in os.listdir(card):                    # snatcherLeo's playthrough starts from New Game on an empty card
            os.remove(os.path.join(card, f))
    print(f"made {user}\nreplay-critical settings: {json.dumps(critical(os.path.join(user, 'Config')), indent=1)}")


def cmd_start(a):
    user = a.user or os.path.join(a.root, "dolphin-user")
    if not os.path.isdir(user):
        sys.exit(f"no Dolphin settings folder {user}: run `setup` first")
    sd = sittings_dir(a.root)                         # holds index.jsonl, the list of every sitting
    n = 1 + max([k for k, _ in all_sittings(a.root)] or [0])
    out = os.path.join(section_dir(a.root, a.section),
                       "%03d_%s" % (n, datetime.datetime.now().strftime("%Y%m%d_%H%M")))
    os.makedirs(out)
    shutil.copytree(os.path.join(user, "Config"), os.path.join(out, "settings"))
    if os.path.isdir(os.path.join(user, CARD_REL)):
        shutil.copytree(os.path.join(user, CARD_REL), os.path.join(out, "card_start"))
    crit = critical(os.path.join(user, "Config"))
    prev = [json.loads(l) for l in open(os.path.join(sd, "index.jsonl"))] if os.path.exists(
        os.path.join(sd, "index.jsonl")) else []
    if prev and prev[-1].get("critical") != crit:
        print("WARNING: replay-critical settings changed since the last sitting:")
        for k, v in crit.items():
            if prev[-1]["critical"].get(k) != v:
                print(f"  {k}: {prev[-1]['critical'].get(k)} -> {v}")
    state = os.path.abspath(a.state) if a.state else ""
    if state:
        shutil.copy2(state, os.path.join(out, "start.sav"))
        state = os.path.join(out, "start.sav")
    meta = dict(n=n, section=a.section.upper(), folder=os.path.relpath(out, a.root), started=datetime.datetime.now().isoformat(timespec="seconds"),
                start="state" if state else "power-on", note=a.note or "", critical=crit)
    if GAME == "xd":
        # what the replays must match, beyond Dolphin.ini: the game INIs' overrides and the Dolphin build itself
        sha = hashlib.sha1(open(DOLPHIN, "rb").read()).hexdigest() if os.path.exists(DOLPHIN) else "missing"
        meta.update(game="xd", game_id=GAME_IDS["xd"], game_ini=game_ini_overrides(user), dolphin_sha1=sha)
        if sha != PINNED_DOLPHIN_SHA1:
            print(f"WARNING: Dolphin is not the pinned build (sha1 {sha[:12]}, pinned {PINNED_DOLPHIN_SHA1[:12]})")
        if prev and prev[-1].get("game_ini") is not None and prev[-1]["game_ini"] != meta["game_ini"]:
            print(f"WARNING: the game INI overrides changed since the last sitting: {prev[-1]['game_ini']} -> "
                  f"{meta['game_ini']}")
        print(f"XD settings the game's INI files set over Dolphin.ini: {meta['game_ini'] or 'none'}")
    json.dump(meta, open(os.path.join(out, "meta.json"), "w"), indent=1)
    print(f"sitting {n}: {out}")
    # the "snatcherLeo is playing" marker (as build-pc/run_game.ps1 writes it): saferun.py holds every heavy run meanwhile
    playing = os.path.join(os.path.dirname(REPO), ".orre_player_playing")
    if not a.drive:
        open(playing, "w").write(f"{os.getpid()} playing a recorded sitting since "
                                 f"{datetime.datetime.now().strftime('%H:%M:%S')}\n")
    movie_path = os.path.join(out, "sitting.dtm")
    if not a.drive:
        print("\n".join([
            "", "=" * 78,
            "DOLPHIN'S OWN MOVIE RECORDING (the exact presses):",
            "  1. As soon as the game window shows: Movie -> Start Recording Input (port 1 only), OK.",
            "  2. Play. LB+RB+left trigger = a mark.",
            "  3. To stop: save at a PC if you can, then LB+RB+Start (Dolphin closes after 3 s).",
            "     The movie is collected automatically (no export needed); it goes to:",
            f"     {movie_path}",
            "=" * 78, ""]), flush=True)
    # No simulated key presses (snatcherLeo, 2026-10-19): a synthetic key goes to whatever window is active, not to Dolphin
    # (the auto-start tests pressed F12 into Edge four times). The movie is started by hand; the recorder's in-game
    # check (dolphin_playrec.movie_check) shows "Movie OK" or a red "NO MOVIE" a few seconds after the logo.
    try:
        rc = run_dolphin(user, out, "record", state=state, pad=os.path.abspath(a.drive) if a.drive else "",
                         batch=bool(a.drive))
    finally:
        if not a.drive and os.path.exists(playing):
            os.remove(playing)
    if os.path.isdir(os.path.join(user, CARD_REL)):
        shutil.copytree(os.path.join(user, CARD_REL), os.path.join(out, "card_end"))
    sj = os.path.join(out, "sitting.json")
    if not os.path.exists(sj):                    # window closed without the end combo: last frame from the log
        last = 0
        evp = os.path.join(out, "events.jsonl")
        if not os.path.exists(evp):
            sys.exit(f"the recorder script wrote nothing (see {out}\\script_error.txt if present)")
        for line in open(evp):
            last = max(last, json.loads(line).get("f", 0))
        json.dump({"last_frame": last, "reason": "window closed", "mode": "record"}, open(sj, "w"))
    meta.update(json.load(open(sj)), ended=datetime.datetime.now().isoformat(timespec="seconds"), exit=rc)
    json.dump(meta, open(os.path.join(out, "meta.json"), "w"), indent=1)
    dtms = [x for x in os.listdir(out) if x.lower().endswith(".dtm")]
    # No export needed: while a movie records, Dolphin writes the movie so far next to every save state the recorder
    # makes (<state>.sav.dtm), so the end state's copy is the whole movie; a movie started with the game running begins
    # from the state Dolphin saves at that moment (User/StateSaves/dtm.sav), which playback needs as <movie>.sav
    # (verified on sitting 2, 2026-10-18).
    end_dtm = os.path.join(out, "states", "end.sav.dtm")
    start_sav = os.path.join(user, "StateSaves", "dtm.sav")
    t_start = datetime.datetime.fromisoformat(meta["started"]).timestamp()
    if os.path.exists(start_sav) and os.path.getmtime(start_sav) < t_start - 5:
        start_sav = ""                                # a previous sitting's: this one recorded no movie
    if not dtms and os.path.exists(end_dtm):
        shutil.copy2(end_dtm, movie_path)
        if start_sav:
            shutil.copy2(start_sav, movie_path + ".sav")
        dtms = ["sitting.dtm"]
    elif dtms and not os.path.exists(os.path.join(out, dtms[0] + ".sav")) and start_sav:
        shutil.copy2(start_sav, os.path.join(out, dtms[0] + ".sav"))
    meta["movie"] = dtms[0] if dtms else None
    json.dump(meta, open(os.path.join(out, "meta.json"), "w"), indent=1)
    with open(os.path.join(sd, "index.jsonl"), "a") as f:
        f.write(json.dumps(meta) + "\n")
    if GAME == "xd":
        # no report or check-out words yet: both read Colosseum's events (rooms, battles, flags), which XD's recorder
        # doesn't log until XD's addresses are known
        print(f"XD sitting {n}: {meta.get('last_frame')} frames, {meta.get('marks', 0)} marks, ended by "
              f"{meta.get('reason')}; Dolphin movie: {meta['movie'] or 'NONE SAVED (no exact replay possible)'}")
        print(f"next: python -I tools/local/testing/playrec.py --game xd replay {n}   (twice; each compares itself "
              f"with the earlier replays' RAM hashes)")
        return
    report(out)
    if not a.drive:
        print(f"Dolphin movie: {dtms[0] if dtms else 'NONE SAVED (the presses are only in pad.jsonl, one per frame)'}")
        checkout(out, meta)


def checkout(out, meta):
    """The check-out after the end combo (snatcherLeo's part E, typed instead of said, 2026-10-19): a word per mark,
    anything odd, plan items skipped. Filed as checkout.json and appended to the sitting's report."""
    if not sys.stdin or not sys.stdin.isatty():
        return
    marks = [json.loads(l) for l in open(os.path.join(out, "events.jsonl")) if '"mark"' in l] \
        if os.path.exists(os.path.join(out, "events.jsonl")) else []
    print("\n" + "=" * 78 + "\nCheck-out (press Enter to skip a question):")
    ans = {"code": CODE_RE.match(meta.get("note", "")).group(1) if CODE_RE.match(meta.get("note", "")) else "",
           "marks": {}}
    planned = bool(ans["code"])                     # a free sitting (no plan code) is defined by these answers
    try:
        if not planned:
            ans["did"] = input("  What did you do this sitting? ").strip() or "(not said)"
        for m in marks:
            ans["marks"][str(m["n"])] = input(f"  Mark {m['n']} (frame {m['f']}): one word? ").strip()
        ans["odd"] = input("  Anything odd? ").strip() or "nothing"
        if planned:
            ans["skipped"] = input("  Plan items skipped? ").strip() or "none"
            ans["saved_at"] = input("  Saved at (Enter = the plan's place)? ").strip() or "the plan's place"
        else:
            ans["saved_at"] = input("  Saved at (where)? ").strip() or "(not said)"
    except (EOFError, KeyboardInterrupt):
        print("\n(check-out skipped)")
        return
    json.dump(ans, open(os.path.join(out, "checkout.json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    lines = ["", "## Check-out (snatcherLeo)"]
    if "did" in ans:
        lines.append(f"- what this sitting did: {ans['did']}")
    lines.append(f"- saved at: {ans['saved_at']}")
    lines += [f"- mark {k}: {v or '(no word)'}" for k, v in ans["marks"].items()]
    lines.append(f"- anything odd: {ans['odd']}")
    if "skipped" in ans:
        lines.append(f"- plan items skipped: {ans['skipped']}")
    with open(os.path.join(out, "report.md"), "a", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    meta["checkout"] = ans
    json.dump(meta, open(os.path.join(out, "meta.json"), "w"), indent=1)
    print("Thanks: filed in the sitting's report. Tell Main it's done (or Main sees it in the report).")


# --- run 2's launcher: `playrec.py next` (2026-10-19; snatcherLeo approved the recorder set: "double-click, play, end
# combo, type a few words"). The plan is the playthrough guide's part 8 table; the pre-flight is its part 3.
PINNED_DOLPHIN_SHA1 = "21751b64b4776f2ce6f1296aa8e694d7d4906606"   # Felk's fork, preview4 x64 (snatcherLeo, 2026-10-19)
GUIDE = os.path.join(REPO, "docs", "testing", "playthrough-guide.md")   # the final guide (the v2 draft is retired)
CODE_RE = re.compile(r"^(P\d+[a-z]?|BM\d*|SYS|EVO)\b")
# the recording profile's Hotkeys.ini, approved by snatcherLeo 2026-10-19: a profile with no keys, so Dolphin's keyboard
# hotkeys (F1-F8 states, Esc, ...) are off while recording. Not a replay-critical setting (critical() reads only
# Dolphin.ini), checked on its own by the pre-flight.
HOTKEYS_LINES = ["[Hotkeys1]", "Device = DInput/0/Keyboard Mouse"]


def plan_rows(guide=None):
    """[(code, section, what, save point)] in plan order from the guide's part 8 table ("P14-P16" is expanded)."""
    rows, in_part8 = [], False
    for line in open(guide or GUIDE, encoding="utf-8"):
        if line.startswith("## "):
            in_part8 = line.startswith("## 8.")
            continue
        cells = [c.strip().replace("**", "") if k == 0 else c.strip()
                 for k, c in enumerate(line.strip().strip("|").split("|"))] if in_part8 and line.startswith("|") else []
        if len(cells) < 5 or not re.match(r"P\d+|BM|SYS|EVO", cells[0]):
            continue
        code, sec, what, save = cells[0], cells[1].split("/")[0], cells[2].replace("**", ""), cells[-1].replace("**", "")
        m = re.match(r"(P|BM)(\d+)-(?:P|BM)?(\d*)$", code)
        if m:   # "P14-P16", "BM1-BM5"; an open end ("P26-", the postgame: one sitting each) runs to P40
            lo, hi = int(m.group(2)), int(m.group(3)) if m.group(3) else 40
            codes = [f"{m.group(1)}{k:02d}" if m.group(1) == "P" else f"BM{k}" for k in range(lo, hi + 1)]
        else:
            codes = [code]
        for c in codes:
            # a sitting placed in the story order (snatcherLeo, 2026-10-21: "BM1 first, then P04") keeps its first place; the
            # general "BM1-BM5" row later in the table doesn't add it again
            if all(r[0] != c for r in rows):
                rows.append((c, sec, what, save))
    return rows


def done_codes(root):
    """Plan codes of the sittings that ended with the end combo (meta.json's note starts with the code)."""
    out = set()
    for _n, folder in all_sittings(root):
        mp = os.path.join(folder, "meta.json")
        if not os.path.exists(mp):
            continue
        meta = json.load(open(mp))
        m = CODE_RE.match(meta.get("note", ""))
        # done = ended with the end combo AND a Dolphin movie was saved (2026-10-20: a P01 attempt that ended with
        # "NO MOVIE" counted as done and the shortcut offered P02)
        if m and meta.get("reason") == "end combo" and meta.get("movie"):
            out.add(m.group(1))
    return out


def card_summary(user):
    """(text, ok) for the save on the recording card: trainer name, play time, the room it resumes in."""
    card = os.path.join(user, CARD_REL)
    gcis = sorted(glob.glob(os.path.join(card, "*.gci"))) if os.path.isdir(card) else []
    if not gcis:
        return "the card is empty (a New Game)", True
    sys.path.insert(0, os.path.join(REPO, "tools", "local"))
    from colo import savefile
    try:
        block = savefile.current_block(open(gcis[0], "rb").read())
    except Exception as e:
        return f"{os.path.basename(gcis[0])}: unreadable ({e})", False
    if block is None:
        return f"{os.path.basename(gcis[0])}: no valid save slot", False
    # save header (gamedatasaveBios*, section 1 at block +0x08): +0x08 save count, +0x14 room id, +0x18 play time
    # (f32 seconds, likely); verified 2026-10-19 on run 1's Phenac save (count 1, room 4 = the Pokémon Center)
    count, room = struct.unpack(">I", block[0x10:0x14])[0], struct.unpack(">I", block[0x1C:0x20])[0]
    secs = struct.unpack(">f", block[0x20:0x24])[0]
    name = room_name(room)
    return (f"{savefile.player_name(block)}, play time {int(secs // 3600)}:{int(secs % 3600 // 60):02d}, saved "
            f"{count} time(s), resumes in room {room:#x}{' (' + name + ')' if name else ''}"), True


def room_name(room):
    for base in (REPO, os.path.join(os.path.dirname(REPO), "pkmn_colosseum")):
        p = os.path.join(base, "extracted", "data_tables", "rooms_decoded.csv")
        if os.path.exists(p):
            import csv
            for r in csv.DictReader(open(p, encoding="utf-8")):
                if r.get("room_id") and int(r["room_id"]) == room:
                    return " / ".join(x for x in (r.get("area"), r.get("archive")) if x)
    return ""


def preflight(a, code):
    """The guide's part 3 pre-flight. Returns True when every line is OK."""
    user = a.user or os.path.join(a.root, "dolphin-user")
    lines = []
    sha = hashlib.sha1(open(DOLPHIN, "rb").read()).hexdigest() if os.path.exists(DOLPHIN) else "missing"
    lines.append((sha == PINNED_DOLPHIN_SHA1, f"Dolphin build (pinned preview4): {sha[:12]}"))
    lines.append((os.path.exists(ISO), f"disc image: {ISO}"))
    crit = critical(os.path.join(user, "Config")) if os.path.isdir(user) else None
    idx = os.path.join(sittings_dir(a.root), "index.jsonl")
    prev = [json.loads(l) for l in open(idx)] if os.path.exists(idx) else []
    changed = [k for k, v in (crit or {}).items() if prev and prev[-1].get("critical", {}).get(k) != v]
    lines.append((crit is not None and not changed, "replay-critical settings: " +
                  ("unchanged since the last sitting" if not changed else "CHANGED: " + ", ".join(changed))))
    # the no-hotkeys file (snatcherLeo, 2026-10-19: "Have the pre-flight check that Hotkeys.ini is present, so the
    # protection can't silently disappear"): exactly these two lines, whitespace and line endings tolerated
    hk = os.path.join(user, "Config", "Hotkeys.ini")
    hk_lines = [l.strip() for l in open(hk, encoding="utf-8", errors="replace").read().splitlines() if l.strip()] \
        if os.path.exists(hk) else None
    hk_ok = hk_lines is not None and [re.sub(r"\s*=\s*", " = ", l) for l in hk_lines] == HOTKEYS_LINES
    lines.append((hk_ok, "keyboard hotkeys off (Hotkeys.ini as approved)" if hk_ok else
                  "the no-hotkeys file is missing or changed: tell Main"))
    text, ok = card_summary(user)
    if code == "P01" and "empty" not in text:
        ok, text = False, text + " -- P01 needs an empty card"
    if code.startswith("BM") and "empty" in text:   # battle mode registers its team from the E-BM save on this card
        ok, text = False, "the battle-mode card is empty -- Main puts the E-BM save there first"
    lines.append((ok, f"save on the card: {text}"))
    busy = subprocess.run(["tasklist"], capture_output=True, text=True).stdout.lower()
    others = [x for x in ("dolphin.exe", "orrecolo.exe") if x in busy]
    lines.append((not others, "nothing else running" if not others else "running now: " + ", ".join(others)))
    free = shutil.disk_usage(a.root).free / 2 ** 30
    lines.append((free >= 5, f"free disk space: {free:.0f} GB"))
    for ok, txt in lines:
        print(("  OK    " if ok else "  STOP  ") + txt)
    return all(ok for ok, _ in lines)


def cmd_next(a):
    plan = plan_rows()
    done = done_codes(a.root)
    if a.code:
        row = next((r for r in plan if r[0] == a.code), None)
    elif getattr(a, "bm", False):     # the battle-mode shortcut: the next BM sitting, whatever the story is at
        row = next((r for r in plan if r[0].startswith("BM") and r[0] not in done), None)
    else:
        # the story shortcut follows the guide's part 8 order, including a BM sitting the table places between story
        # sittings (BM1 before P04, snatcherLeo 2026-10-21); the other BM sittings come from the battle-mode shortcut (any
        # time) or after the story. A sitting counts as done only with the end combo and a saved movie (done_codes),
        # so an attempt without a recording stays the next one
        inline = set()
        for r in plan:
            if r[0].startswith("BM") and any(p[0].startswith("P") for p in plan[plan.index(r) + 1:]):
                inline.add(r[0])
        row = next((r for r in plan if r[0] not in done and (not r[0].startswith("BM") or r[0] in inline)), None)
    if not row:
        sys.exit(f"no sitting {a.code} in the plan" if a.code else "every sitting in the plan is done")
    code, sec, what, save = row
    if code.startswith("BM") and not a.user:
        # battle mode runs on its own profile and card (E-BM); the pre-flight and the sitting use it
        a.user = os.path.join(a.root, BM_USER)
        if not os.path.isdir(a.user):
            sys.exit(f"{code} needs the battle-mode profile {a.user}: Main makes it (playrec.py setup-bm) and puts the "
                     f"E-BM save on its card")
    print("=" * 78)
    print(f"Next: {code}, {sec}: {what}")
    print(f"Save at: {save}")
    print("=" * 78)
    print("Pre-flight:")
    if not preflight(a, code):
        print("\nSomething above says STOP: don't start. Tell Main what it says.")
        sys.exit(1)
    if a.dry:
        print("\nAll OK (dry run: Dolphin not started).")
        return
    print("\nAll OK. Starting Dolphin.")
    a.section, a.note, a.state, a.drive = sec, f"{code} {a.note or ''}".strip(), None, None
    cmd_start(a)


def cmd_free(a):
    """A sitting with no plan code (snatcherLeo, 2026-10-24: "We don't need to call it anything, and the comments I give
    after the recording is over are what define what I did"): the pre-flight, then a recording filed under FREE; the
    check-out asks what the sitting did."""
    bm = getattr(a, "bm", False)        # battle mode: its own profile and card (2026-10-24, BM2-BM3 done in one sitting)
    if bm and not a.user:
        a.user = os.path.join(a.root, BM_USER)
    print("=" * 78)
    print(("Free battle-mode sitting" if bm else "Free sitting") + ": no plan code; after the end combo, say what you did.")
    print("=" * 78)
    print("Pre-flight:")
    if not preflight(a, "BM" if bm else "FREE"):
        print("\nSomething above says STOP: don't start. Tell Main what it says.")
        sys.exit(1)
    if a.dry:
        print("\nAll OK (dry run: Dolphin not started).")
        return
    print("\nAll OK. Starting Dolphin.")
    a.section, a.note, a.state, a.drive = ("BM" if bm else "FREE"), a.note or "", None, None
    cmd_start(a)


def cmd_shortcut(a):
    """A double-click launcher on the Desktop for `playrec.py free` (default since 2026-10-24), `next` (--plan) or the
    battle-mode `next --bm` (snatcherLeo's part: double-click, play, end combo)."""
    bm = getattr(a, "bm", False)
    plan = getattr(a, "plan", False)
    name = "OrrePort battle mode sitting.cmd" if bm else "OrrePort next sitting.cmd"
    path = a.path or os.path.join(os.path.expanduser("~"), "Desktop", name)
    what = ("next --bm" if plan else "free --bm") if bm else "next" if plan else "free"
    open(path, "w", newline="\r\n").write("\n".join([
        "@echo off",
        "rem Made by playrec.py shortcut: the next recorded " + ("battle-mode sitting" if bm else "sitting from the plan"
        if plan else "sitting (no plan code: the check-out says what it did)") + ", with its pre-flight.",
        f'cd /d "{REPO}"',
        "python -I tools\\local\\testing\\playrec.py " + what,
        "echo.",
        "pause", ""]))
    print(f"made {path}")


AFTER_PROBES = "input,pos,rng,windows,dvd,card,frames"   # what replay_check's recipe reads (guide Appendix C)


def cmd_after(a):
    """The one-command chain (guide part 11 item 2, 2026-10-26): the Dolphin replay with the recipe's probes (its
    check against the recording runs inside it), then replay_check.py on that replay (the timing files, the PC replay,
    the report), then one line: "<code>: passes" or the first difference, with the open gaps (snatcherLeo's tolerance rule).
    Both runs go through saferun --exclusive, so the chain waits by itself while snatcherLeo plays or records."""
    sit = find_sitting(a.root, a.n)
    meta = json.load(open(os.path.join(sit, "meta.json"), encoding="utf-8")) if os.path.exists(
        os.path.join(sit, "meta.json")) else {}
    m = CODE_RE.match(meta.get("note", ""))
    code = m.group(1) if m else os.path.basename(sit)
    main_root = os.path.join(os.path.dirname(REPO), "pkmn_colosseum")
    saferun = os.path.join(main_root, "tools", "local", "saferun.py")
    log = os.path.join(os.path.dirname(REPO), "_d_testing", f"after_{os.path.basename(sit)}.log")
    before = {x for x in os.listdir(sit) if re.match(r"replay_\d+$", x)}
    # the trace the PC replays read so far (replay_check takes the newest replay with a dvd.csv): per-sitting
    # ORRE_DVD_READ_FIX rows index its rows, so a new capture that differs must be flagged (2026-10-26)
    old_dvd = next((os.path.join(sit, x, "dvd.csv") for x in sorted(before, key=lambda x: -int(x.split("_")[1]))
                    if os.path.exists(os.path.join(sit, x, "dvd.csv"))), None)
    print(f"[after] {code} ({os.path.basename(sit)}): the Dolphin replay with {a.probes} (waits while snatcherLeo plays)",
          flush=True)
    iso = ISO if os.path.exists(ISO) else os.path.join(main_root, "orig", "Pokemon Colosseum (USA).iso")
    subprocess.run([sys.executable, "-I", saferun, "--exclusive", "--time", "3600", "--why",
                    f"D: playrec after {code}: the Dolphin replay for the chain", "--log", log, "--", sys.executable,
                    "-I", os.path.abspath(__file__), "--root", a.root, "replay", str(a.n), "--probe", a.probes],
                   env=dict(os.environ, PLAYREC_ISO=iso))
    new = sorted((x for x in os.listdir(sit) if re.match(r"replay_\d+$", x) and x not in before),
                 key=lambda x: int(x.split("_")[1]))
    text = open(log, encoding="utf-8", errors="replace").read() if os.path.exists(log) else ""
    if not new or not os.path.exists(os.path.join(sit, new[-1], "replay.json")):
        print(f"{code}: the Dolphin replay didn't finish (log: {log})")
        return 1
    rep = new[-1]
    if "events: all" not in text or "story flags: DIFFER" in text or "switch-ins: DIFFER" in text:
        bad = next((l for l in text.splitlines() if l.startswith(("events:", "switch-ins:", "story flags:"))
                    and ("DIFFER" in l or not l.startswith("events: all"))), "(see the log)")
        print(f"{code}: the Dolphin replay doesn't match the recording ({bad}); {rep} kept for a look, no PC replay")
        return 1
    trace_note = ""
    new_dvd = os.path.join(sit, rep, "dvd.csv")
    if old_dvd and os.path.exists(new_dvd) and open(old_dvd, "rb").read() != open(new_dvd, "rb").read():
        trace_note = (f"; NOTE: {rep}'s dvd.csv differs from {os.path.basename(os.path.dirname(old_dvd))}'s, and the "
                      f"PC replays now read it: re-check any ORRE_DVD_READ_FIX rows for this sitting")
        print(f"[after] {code}{trace_note}", flush=True)
    print(f"[after] {code}: {rep} matches the recording; the PC replay (replay_check.py)", flush=True)
    out = os.path.join(os.path.dirname(REPO), "_d_testing", "replay_check",
                       f"after_{os.path.basename(sit)}_{rep}")
    cmd = [sys.executable, "-I", os.path.join(HERE, "replay_check.py"), sit, "--replay", rep.split("_")[1],
           "--out", out] + (a.check_args.split() if a.check_args else [])
    subprocess.run(cmd)
    report = os.path.join(out, "report.md")
    lines = open(report, encoding="utf-8").read().splitlines() if os.path.exists(report) else []
    first = next((l.split(":", 1)[1].strip().rstrip(".") for l in lines if l.startswith("- First difference:")), None)
    gaps = next((l.split(":", 1)[1].strip() for l in lines if l.startswith("- Open gaps")), None)
    if first is None:
        print(f"{code}: no report (see {out})")
        return 1
    verdict = "passes" if first.startswith("none") else f"first difference: {first}"
    print(f"{code}: {verdict}" + (f"; open gaps: {gaps}" if gaps else "") + trace_note + f" (report: {report})")
    return 0


def cmd_replay(a):
    sit = find_sitting(a.root, a.n)
    # after the highest number, not the count (2026-10-25): with replay_29 deleted the count gave 31 and the next
    # capture went into the existing replay_31 (its vipos / rng files kept, owners added)
    k = 1 + max((int(m.group(1)) for x in os.listdir(sit) for m in [re.match(r"replay_(\d+)$", x)] if m), default=0)
    out = os.path.join(sit, "replay_%d" % k)
    user = os.path.join(out, "user")
    copy_user(a.user or os.path.join(a.root, "dolphin-user"), user)
    shutil.rmtree(os.path.join(user, "Config"))
    shutil.copytree(os.path.join(sit, "settings"), os.path.join(user, "Config"))
    card = os.path.join(user, CARD_REL)
    shutil.rmtree(card, ignore_errors=True)
    if os.path.isdir(os.path.join(sit, "card_start")):
        shutil.copytree(os.path.join(sit, "card_start"), card)
    else:
        os.makedirs(card)
    gcpad = os.path.join(user, "Config", "GCPadNew.ini")
    if os.path.exists(gcpad):
        os.remove(gcpad)                              # no real controller during a replay
    if os.environ.get("PLAYREC_OSREPORT") == "1":
        # the game's own log lines (OSReport / GSlogWrite: "check event = 2", "loadArchivePtr", ...) into
        # <replay>/dolphin.log, to set beside the PC's (2026-10-21). Only this replay's copied settings change.
        # Compare the replay's rng.csv with a clean replay's before trusting it (a memory watch once disturbed Dolphin).
        # Verified 2026-10-21: the logger leaves the replay undisturbed (RNG identical on all 43,144 frames of P01), but
        # the retail game's OSReport is silent: only the apploader's and the SDK's banners appear, no GSlog lines
        with open(os.path.join(user, "Config", "Logger.ini"), "w", encoding="utf-8") as f:
            f.write("[Options]\nVerbosity = 4\nWriteToFile = True\nWriteToConsole = False\nWriteToWindow = False\n"
                    "[Logs]\nOSREPORT = True\nOSREPORT_HLE = True\nMI = True\nPOWERPC = True\n")
            # "MI" is this build's key for the memory-map log (its own "Memory system initialized" line logs as I[MI]);
            # "MEMMAP = True" was ignored (2026-10-21, verified)
    breaks = [b for b in os.environ.get("PLAYREC_BREAKS", "").split(",") if b]
    if breaks:
        # PLAYREC_BREAKS=<hex addr>[,...] (2026-10-20): more log-only code breakpoints ("$addr nl", as the
        # profile's idlog ones) in this replay's copy of GameSettings/GC6E01.ini. P02: does the console also print
        # "_eyeTexAnimEnded: Wrong action value!" (0x801DEDE4)? *Corrected 2026-10-21:* the hits never reach
        # dolphin.log in a -b run, even with the MI log on (verified on a boot with 0x800FF58C); dolphin_playrec.py's
        # code-breakpoint handler writes them to replay_events.jsonl ("break" events) instead, and only a first hit is
        # reliable (a "break" event means the code ran at least once by then, not how often)
        ini = os.path.join(user, "GameSettings", "GC6E01.ini")
        os.makedirs(os.path.dirname(ini), exist_ok=True)
        text = open(ini, encoding="utf-8").read() if os.path.exists(ini) else "[BreakPoints]\n"
        if "[BreakPoints]" not in text:
            text += "\n[BreakPoints]\n"
        text = text.rstrip("\n") + "\n" + "".join("$%s nl\n" % b.lower().removeprefix("0x") for b in breaks)
        open(ini, "w", encoding="utf-8").write(text)
    if os.environ.get("PLAYREC_DEBUGMODE") == "1":
        # PLAYREC_DEBUGMODE=1 (2026-10-25): Dolphin's debug mode ([Interface] DebugModeEnabled) in this replay's
        # copied Dolphin.ini only. Without it the JIT ignores code breakpoints (MAIN_ENABLE_DEBUGGING; P01's replay_28:
        # no "BP" lines at all). Debug mode may change how the JIT runs: compare rng.csv with a clean replay first.
        # *Corrected 2026-10-26 (verified):* the recording profiles (_playthrough/dolphin-user and dolphin-user-bm)
        # already have DebugModeEnabled = True, and a replay copies the sitting's own settings, so recordings and
        # replays already run in debug mode and this switch changes nothing for them. Code breakpoints do load and
        # fire there (P02's replay_8: 0x801DEDE4 at frame 16490, RNG identical on all 83,127 frames); replay_28 had no
        # "BP" lines because a -b run writes no breakpoint hit to dolphin.log (the fork's Python event reports them,
        # first hit only: dolphin_playrec.py's PLAYREC_BREAKS handler)
        ini = os.path.join(user, "Config", "Dolphin.ini")
        text = open(ini, encoding="utf-8").read() if os.path.exists(ini) else ""
        new, in_iface, done = [], False, False   # not "out": that is the replay folder
        for line in text.splitlines():
            if line.strip().startswith("["):
                if in_iface and not done:
                    new.append("DebugModeEnabled = True")
                    done = True
                in_iface = line.strip() == "[Interface]"
            elif in_iface and line.split("=")[0].strip() == "DebugModeEnabled":
                line, done = "DebugModeEnabled = True", True
            new.append(line)
        if not done:
            new += ([] if in_iface else ["[Interface]"]) + ["DebugModeEnabled = True"]
        open(ini, "w", encoding="utf-8").write("\n".join(new) + "\n")
    dtms = sorted(x for x in os.listdir(sit) if x.lower().endswith(".dtm"))
    if a.movie:
        # --movie <file> (2026-11-01, the reference playthrough Part 1 item 4): another movie of this sitting, such as one converted from
        # a PC recording by orrepad_to_dtm.py, played on the sitting's settings and start card; the check compares it
        # with the sitting's own recording as usual
        # The movie is copied into the replay folder with the sitting's start state beside it: a sitting recorded with
        # the game already running begins from the state Dolphin saved then (<movie>.sav, as above), and without it
        # Dolphin boots from power-on and every input comes ~800 frames early (P25's first converted pass,
        # 2026-11-01: the inputs fell in the boot and the game waited in room 0x384)
        with open(os.path.join(out, "movie.txt"), "w", encoding="utf-8") as f:
            f.write(os.path.abspath(a.movie) + "\n")
        own = [x for x in dtms if os.path.exists(os.path.join(sit, x + ".sav"))]
        dtms = [os.path.join(out, "movie.dtm")]
        shutil.copy2(a.movie, dtms[0])
        if own:
            shutil.copy2(os.path.join(sit, own[0] + ".sav"), dtms[0] + ".sav")
    if a.livepad:
        # --livepad <steps.json> (2026-11-02; the reference playthrough's Dolphin side): no movie; the script feeds the PC's pad live,
        # each change timed from the event it follows as Dolphin shows it (livepad_steps.py, dolphin_playrec.py
        # PLAYREC_LIVEPAD). From power-on with the sitting's settings and start card
        shutil.copy2(a.livepad, os.path.join(out, "livepad_steps.json"))
        last = json.load(open(os.path.join(sit, "sitting.json")))["last_frame"]
        rc = run_dolphin(user, out, "watch", speed=a.speed,
                         extra_env={"PLAYREC_FRAMES": str(last + 600), "PLAYREC_PROBES": a.probe or "",
                                    "PLAYREC_LIVEPAD": os.path.join(out, "livepad_steps.json")})
        shutil.rmtree(user, ignore_errors=True)
        if rc == 0x57:
            sys.exit(0x57)              # the no-progress watchdog ended it: callers (saferun, run3_live) see it
        return
    if dtms and not a.pad:                            # the exact path: Dolphin plays its own movie, the script watches
        last = json.load(open(os.path.join(sit, "sitting.json")))["last_frame"]
        if a.movie:
            with open(dtms[0], "rb") as f:
                last = max(last, struct.unpack("<Q", f.read(0x15)[0x0D:0x15])[0])   # the movie's field count
        # PLAYREC_FRAMES set by the caller (2026-11-06) ends the replay sooner, for shots of one scene in a long
        # sitting; the event check after it then reports the events past the cap as missing
        cap = last + 600
        if os.environ.get("PLAYREC_FRAMES", "").isdigit() and int(os.environ["PLAYREC_FRAMES"]) > 0:
            cap = min(cap, int(os.environ["PLAYREC_FRAMES"]))
        run_dolphin(user, out, "watch", movie=os.path.join(sit, dtms[0]), speed=a.speed,
                    extra_env={"PLAYREC_FRAMES": str(cap), "PLAYREC_PROBES": a.probe or ""})
        log = os.path.join(user, "Logs", "dolphin.log")
        if os.path.exists(log):
            shutil.copy2(log, os.path.join(out, "dolphin.log"))   # PLAYREC_OSREPORT=1
        shutil.rmtree(user, ignore_errors=True)
        if GAME == "xd":
            sys.exit(xd_compare(sit, out))
        check_aligned(sit, out)
        return
    if GAME == "xd":
        sys.exit(f"sitting {a.n} has no Dolphin movie: an XD sitting replays only from its movie (the pad log is a "
                 f"buttons-only backup)")
    state = os.path.join(sit, "start.sav") if os.path.exists(os.path.join(sit, "start.sav")) else ""
    run_dolphin(user, out, "replay", state=state, pad=os.path.join(sit, "pad.jsonl"), speed=a.speed,
                extra_env={"PLAYREC_SHIFT": str(a.shift), "PLAYREC_PROBES": a.probe or ""})   # probes here too (
    # 2026-10-19: sitting 1 has no movie, and its first capture ran without them)
    shutil.rmtree(user, ignore_errors=True)
    check(sit, out)


def xd_compare(sit, out):
    """An XD replay against the sitting's earlier replays, by their RAM hashes (dolphin_xdrec.py's hashes.csv: all 24 MB
    of main RAM every few thousand frames and at the end). Why: XD's addresses aren't known yet, so "the same result"
    is the whole memory being the same (Colosseum compared events instead). Two replays of one movie with identical
    hashes prove the sitting replays exactly. Returns 0 when every earlier replay matches, 1 on a difference, 2 when
    this replay has no hashes."""
    def load(d):
        p = os.path.join(d, "hashes.csv")
        if not os.path.exists(p):
            return None
        rows = [l.strip().split(",") for l in open(p) if l.strip() and not l.startswith("frame")]
        return [(int(f), h) for f, h in rows]
    mine = load(out)
    if not mine:
        print(f"{os.path.basename(out)}: no RAM hashes (see replay_events.jsonl for a hash_error)")
        return 2
    others = sorted((int(m.group(1)), os.path.join(sit, x)) for x in os.listdir(sit)
                    for m in [re.match(r"replay_(\d+)$", x)] if m and x != os.path.basename(os.path.normpath(out)))
    print(f"{os.path.basename(out)}: {len(mine)} RAM hashes, the last at frame {mine[-1][0]}")
    rc = 0
    for k, d in others:
        theirs = load(d)
        if not theirs:
            continue
        a, b = dict(mine), dict(theirs)
        common = sorted(set(a) & set(b))
        diff = [f for f in common if a[f] != b[f]]
        if diff:
            rc = 1
            print(f"  vs replay_{k}: DIFFER from frame {diff[0]} (the last match before it: "
                  f"{max([f for f in common if f < diff[0]], default='none')}); {len(diff)} of {len(common)} differ")
        else:
            print(f"  vs replay_{k}: identical at all {len(common)} common hashes (the end: {'same' if mine[-1] == theirs[-1] else 'different length'})")
    if not any(load(d) for _, d in others):
        print("  (the first replay: run it again to compare)")
    return rc


ALIGNED = ("entered", "msgwin", "sound", "battle", "battle_result", "hp", "move_used", "switch", "field")
POLLED_10 = ("hp", "move_used", "switch", "field")   # dolphin_playrec.py reads the battle field every 10 frames
SWAP_TOL = 10   # frames: two neighbouring events this close may be logged in either order (the 10-frame field poll)


def check_aligned(sit, rep):
    """A movie playback starts where the movie was started, not at the logger's frame 1: align the two logs on the
    first room entry they share (same room, same random number state), then compare every frame-exact event with the
    frame offset applied (ticks and the 30-frame flag polls fall on other frames after a shift: flags are compared as
    the ordered list of changes)."""
    a = [e for e in map(json.loads, open(os.path.join(sit, "events.jsonl")))]
    b = [e for e in map(json.loads, open(os.path.join(rep, "replay_events.jsonl")))]
    ea = [e for e in a if e["ev"] == "entered"]
    eb = [e for e in b if e["ev"] == "entered"]
    if not eb:
        print("events: the playback entered no room")
        return False
    start = next((x for x in ea if x["room"] == eb[0]["room"] and x["rng"] == eb[0]["rng"]), None)
    if start is None:
        print(f"events: the playback's first room entry ({eb[0]}) is not in the recording")
        return False
    off = start["f"] - eb[0]["f"]
    last = json.load(open(os.path.join(sit, "sitting.json")))["last_frame"]
    strip = lambda e, o: {k: v for k, v in dict(e, f=e["f"] + o).items() if k not in ("t", "n")}
    # "switch" rows are compared per battle as a set of species switched in, not one by one (2026-10-26; P12's
    # double battle): the field is polled every 10 frames at a phase that differs between the logs, and the slot array
    # is re-sorted into turn order (likely), so one poll can show slots 2 / 3 before the re-sort and the other after
    # it, and log different "switch" rows for the same battle
    xa = [strip(e, 0) for e in a if e["ev"] in ALIGNED and e["ev"] != "switch" and start["f"] <= e["f"] <= last]
    xb = [strip(e, off) for e in b if e["ev"] in ALIGNED and e["ev"] != "switch" and start["f"] <= e["f"] + off <= last]

    def switch_ins(evs, o):
        """{battle number in order: set of species switched in}"""
        out, k = {}, -1
        for e in evs:
            if not start["f"] <= e["f"] + o <= last:
                continue
            if e["ev"] == "battle":
                k += 1
            elif e["ev"] == "switch" and e.get("species"):   # species 0: an empty slot caught mid-change by the poll
                out.setdefault(k, set()).add(e["species"])
        return out
    sw_a, sw_b = switch_ins(a, 0), switch_ins(b, off)
    # a flag cleared and set back (or set and cleared) within one poll is seen only when a poll falls inside the gap,
    # so only one log may have it (P17b, 2026-10-27: five purification flags 3/35-36 cleared for 30 frames as a message
    # window opened, in the replay only): a blip only one log saw is dropped before comparing
    a, blips_a = drop_flag_blips(a, 0, b, off)
    b, blips_b = drop_flag_blips(b, off, a, 0)
    fa = [(e["cls"], e["word"], e["bit"], e["now"]) for e in a if e["ev"] == "flag" and start["f"] <= e["f"] <= last]
    fb = [(e["cls"], e["word"], e["bit"], e["now"]) for e in b if e["ev"] == "flag" and start["f"] <= e["f"] + off <= last]
    print(f"aligned on room {start['room']:#x} (recording frame {start['f']}, playback frame {eb[0]['f']})")
    ok = True

    def same(x, y):
        if x["ev"] in POLLED_10 and y["ev"] == x["ev"] and abs(x["f"] - y["f"]) < 10:
            return dict(x, f=0) == dict(y, f=0)      # sampled every 10 logger frames: the shift moves the sample
        return x == y

    swaps = []
    nofr = lambda e: json.dumps({k: v for k, v in e.items() if k != "f"}, sort_keys=True)
    i, n = 0, min(len(xa), len(xb))
    while i < n:
        x, y = xa[i], xb[i]
        if same(x, y):
            i += 1
            continue
        # neighbouring events logged in another order within a few frames (P01, 2026-10-20: the 10-frame field poll's
        # HP / move events landing before or after a message window): the same events in a short window, ignoring
        # order and exact frame, are a note, not a difference
        k = 1
        while i + k < len(xa) and xa[i + k]["f"] - x["f"] <= SWAP_TOL:
            k += 1
        a_blk, b_blk = xa[i:i + k], xb[i:i + k]
        if len(b_blk) == k and sorted(map(nofr, a_blk)) == sorted(map(nofr, b_blk)) \
                and all(abs(p["f"] - q["f"]) <= SWAP_TOL for p, q in zip(sorted(a_blk, key=nofr), sorted(b_blk, key=nofr))):
            swaps.append((i, k, x["f"]))
            i += k
            continue
        print(f"events: first difference at event {i} of {len(xa)}:\n  recorded: {x}\n  replayed: {y}")
        ok = False
        break
    else:
        if len(xa) != len(xb):
            print(f"events: identical for {min(len(xa), len(xb))}, then the counts differ ({len(xa)} vs {len(xb)})")
            ok = False
        else:
            print(f"events: all {len(xa)} identical (rooms with the random-number state, messages, sound, battles, "
                  f"moves, HP)" + (", allowing the reordering noted below" if swaps else ""))
    for i, k, f in swaps:
        print(f"  note: events {i}-{i + k - 1} (around recording frame {f}) were logged in another order within "
              f"{SWAP_TOL} frames (the same events; not a difference)")
    if sw_a == sw_b:
        print(f"switch-ins: the same species in each of {len(sw_a)} battles (compared as sets: the 10-frame field "
              "poll and the slot re-sort make single switch rows differ)")
    else:
        bad = next(k for k in sorted(set(sw_a) | set(sw_b)) if sw_a.get(k) != sw_b.get(k))
        print(f"switch-ins: DIFFER in battle {bad + 1}: recorded {sorted(sw_a.get(bad, []))}, replayed "
              f"{sorted(sw_b.get(bad, []))}")
        ok = False
    same = fa == fb
    if same:
        print(f"story flags: the same {len(fa)} changes in the same order")
    elif sorted(fa) == sorted(fb) and flags_within_poll(a, b, start["f"], last, off):
        # the flags are polled every 30 frames at a phase that differs between the two logs, so two flags set a few
        # frames apart can land in one poll on one side and in two on the other (P02, 2026-10-20: 3/0 bit 2 and 3/27
        # bit 1 at a room change)
        same = True
        print(f"story flags: the same {len(fa)} changes; some seen in another order within one {FLAG_POLL}-frame poll "
              "(the polls' phase; not a difference)")
    else:
        print("story flags: DIFFER")
    for side, bl in (("recording", blips_a), ("replay", blips_b)):
        if bl:
            print(f"  note: the {side} saw {len(bl)} flag(s) change and change back within one {FLAG_POLL}-frame poll "
                  f"(first at its frame {bl[0][0]}: {bl[0][1]}; a poll-phase blip, left out of the comparison)")
    return ok and same


FLAG_POLL = 30   # frames: the probe polls the story flags this often


def drop_flag_blips(evs, o, other, o2):
    """(the events without any flag change undone by the same flag's next change within one poll while the other log
    shows no change of that flag within two polls of it, [(frame, (cls, word, bit))] dropped). A pulse both logs saw,
    one as 30 frames and the other as 60 (P17b's end), stays: flags_within_poll compares those."""
    near = {}
    for e in other:
        if e["ev"] == "flag":
            near.setdefault((e["cls"], e["word"], e["bit"]), []).append(e["f"] + o2)
    flags = [i for i, e in enumerate(evs) if e["ev"] == "flag"]
    drop, nxt, firsts = set(), {}, []
    for i in reversed(flags):
        k = (evs[i]["cls"], evs[i]["word"], evs[i]["bit"])
        j = nxt.get(k)
        if j is not None and j not in drop and evs[j]["f"] - evs[i]["f"] <= FLAG_POLL and evs[j]["now"] != evs[i]["now"] \
                and not any(abs(g - (evs[i]["f"] + o)) <= 2 * FLAG_POLL for g in near.get(k, [])):
            drop |= {i, j}
            firsts.append((evs[i]["f"], k))
        nxt[k] = i
    return [e for i, e in enumerate(evs) if i not in drop], sorted(firsts)


def flags_within_poll(a, b, lo, last, off):
    """Each flag change seen within one poll of the same change on the other side (same flag, same new value, in
    order of occurrence)."""
    def per_flag(evs, o):
        d = {}
        for e in evs:
            if e["ev"] == "flag" and lo <= e["f"] + o <= last:
                d.setdefault((e["cls"], e["word"], e["bit"], e["now"]), []).append(e["f"] + o)
        return d
    da, db = per_flag(a, 0), per_flag(b, off)
    return da.keys() == db.keys() and all(len(da[k]) == len(db[k]) and
                                          all(abs(p - q) <= FLAG_POLL for p, q in zip(da[k], db[k])) for k in da)


def load_events(path):
    out = []
    for line in open(path):
        r = json.loads(line)
        r.pop("t", None)
        if r.get("ev") in ("state", "state_error", "loaded", "power_on", "replay_start", "end", "block"):
            continue
        out.append(r)
    return out


def check(sit, rep):
    chk = [json.loads(l) for l in open(os.path.join(rep, "replay_check.jsonl"))]
    summ = [c for c in chk if c.get("summary")]
    bad = [c for c in chk if not c.get("summary")]
    print(f"inputs: {summ[0]['mismatched_frames'] if summ else '?'} frames where the game read a different pad"
          + (f" (first at frame {bad[0]['f']}: wanted {bad[0]['want']}, got {bad[0]['got']})" if bad else ""))
    a = load_events(os.path.join(sit, "events.jsonl"))
    b = load_events(os.path.join(rep, "replay_events.jsonl"))
    last = json.load(open(os.path.join(sit, "sitting.json")))["last_frame"]
    a = [e for e in a if e["f"] <= last]
    b = [e for e in b if e["f"] <= last]
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            print(f"events: first difference at event {i} of {len(a)}:\n  recorded: {x}\n  replayed: {y}")
            return False
    if len(a) != len(b):
        print(f"events: identical for {min(len(a), len(b))} events, then the counts differ ({len(a)} vs {len(b)})")
        return False
    print(f"events: all {len(a)} identical (rooms, flags, messages, sound, battles, positions, RNG every second)")
    return not bad


def cmd_check(a):
    sit = find_sitting(a.root, a.n)
    reps = sorted((x for x in os.listdir(sit) if x.startswith("replay_")), key=lambda x: int(x.split("_")[1]))
    if not reps:
        sys.exit("no replay yet")
    rep = os.path.join(sit, "replay_%d" % a.replay if a.replay else reps[-1])
    if os.path.exists(os.path.join(rep, "replay_check.jsonl")):
        check(sit, rep)
    else:
        check_aligned(sit, rep)                       # a movie playback (watch mode)


def report(sit):
    ev = [json.loads(l) for l in open(os.path.join(sit, "events.jsonl"))]
    sj = json.load(open(os.path.join(sit, "sitting.json")))
    rooms = [e["room"] for e in ev if e["ev"] == "entered"]
    battles = [e["id"] for e in ev if e["ev"] == "battle"]
    flags = [e for e in ev if e["ev"] == "flag"]
    marks = [e for e in ev if e["ev"] == "mark"]
    moves = [e for e in ev if e["ev"] == "move_used"]
    msgs = [e for e in ev if e["ev"] == "msgwin" and e["type"] >= 0]
    lines = [f"# Sitting {os.path.basename(sit)}",
             f"- frames {sj['last_frame']} (~{sj['last_frame'] / 3600:.1f} min of game time), ended by: {sj['reason']}",
             f"- rooms entered: {len(rooms)} ({len(set(rooms))} different): " + " ".join("%x" % r for r in rooms[:80]),
             f"- battles: {battles}",
             f"- story-flag bits changed: {len(flags)} (set {sum(1 for f in flags if f['now'])})",
             f"- message windows opened: {len(msgs)}; moves used (PP drops): {len(moves)}",
             f"- marks: {len(marks)}"]
    for m in marks:
        lines.append(f"  - mark {m['n']} at frame {m['f']}, room {m['room']:x}, position {m['pos']}")
    states = sorted(os.listdir(os.path.join(sit, "states")))
    lines.append(f"- save states: {len(states)}; save blocks: {len(os.listdir(os.path.join(sit, 'blocks')))}")
    lines += battle_lines(ev, game_names())
    text = "\n".join(lines) + "\n"
    open(os.path.join(sit, "report.md"), "w", encoding="utf-8").write(text)
    print(text)


def game_names():
    """Names from the disc's own tables, as extracted (never from a National Dex list: the game's species numbers are
    its internal ones, docs/engine/pokemon-data.md). Empty dicts when the extraction isn't there."""
    import csv
    names = {"species": {}, "moves": {}, "trainers": {}, "battles": {}, "messages": {}}
    for base in (REPO, os.path.join(os.path.dirname(REPO), "pkmn_colosseum")):
        x = os.path.join(base, "extracted")
        if not os.path.isdir(x):
            continue
        for d in os.listdir(os.path.join(x, "pokemon")) if os.path.isdir(os.path.join(x, "pokemon")) else []:
            m = re.match(r"(\d+)_(.+)$", d)          # "291_Silcoon": internal number _ the game's name
            if m:
                names["species"][int(m.group(1))] = m.group(2).replace("_", " ")
        t = os.path.join(x, "data_tables")
        if os.path.exists(os.path.join(t, "moves_decoded.csv")):
            names["moves"] = {int(r["index"]): r["name"] for r in csv.DictReader(open(os.path.join(t, "moves_decoded.csv"), encoding="utf-8"))}
        if os.path.exists(os.path.join(t, "battles_decoded.csv")):
            names["battles"] = {int(r["battle"]): r for r in csv.DictReader(open(os.path.join(t, "battles_decoded.csv"), encoding="utf-8"))}
        fight = os.path.join(x, "text", "fight_common__fight.txt")
        if os.path.exists(fight):
            for line in open(fight, encoding="utf-8"):
                m = re.match(r"0x([0-9a-fA-F]+)\t(.*)", line)
                if m:
                    names["messages"][int(m.group(1), 16)] = m.group(2)
        break
    return names


def battle_lines(ev, names):
    """The report's battles, named (snatcherLeo, 2026-10-19: names, not numbers): trainers, each Pokémon sent out with its
    moves, the moves used, HP changes, faints, the result, and the game's own battle lines (items, balls, Call, ...)
    with their placeholders shown as <..> (the values are filled at run time and not logged)."""
    sp = lambda n: f"{names['species'].get(n, '?')} ({n})"
    mv = lambda n: names["moves"].get(n) or f"move {n}"
    out, cur, seen, on_field = [], None, set(), {}
    has_send_out = any(e["ev"] == "send_out" for e in ev)
    for e in ev:
        if e["ev"] == "battle":
            b = names["battles"].get(e["id"], {})
            # side_trainers is "id: name; ...": trainer 1 is the player's own record (a placeholder name), left out
            foes = "; ".join(t.split(":", 1)[1].strip() for t in b.get("side_trainers", "").split(";")
                             if ":" in t and t.split(":")[0].strip() != "1")
            out += ["", f"## Battle {e['id']}" + (f" vs {foes}" if foes else ""),
                    f"- starts at frame {e['f']}" + (f", {b.get('arena_name', '')}" if b.get("arena_name") else "")]
            cur, on_field = e["id"], {}
        elif cur is None:
            continue
        elif e["ev"] == "send_out":                   # newer recordings: one line per battle record, slot-proof
            out.append(f"- frame {e['f']}: sends out {sp(e['species'])}, HP {e['hp']}/{e['max_hp']}, moves: "
                       + ", ".join(mv(m) for m in e["moves"] if m))
        elif e["ev"] in ("switch", "field") and not has_send_out:
            for r in ([e["slot"], e["species"], e["hp"], None, e["moves"]],) if e["ev"] == "switch" else e["mons"]:
                key = (e["f"], r[0], r[1])            # old recordings log the opening line-up twice (field + switch)
                if key in seen:
                    continue
                seen.add(key)
                # the slot array is re-sorted into turn order each turn (likely): a species already on the field in
                # another slot has only moved; skip it (only a species not on the field is a real send-out)
                if on_field.get("_f") != e["f"]:      # compare against the line-up before this poll
                    on_field["_before"] = {s: v for s, v in on_field.items() if not str(s).startswith("_")}
                    on_field["_f"] = e["f"]
                moved = any(s != r[0] and sp_id == r[1] for s, sp_id in on_field["_before"].items())
                on_field[r[0]] = r[1]
                if moved:
                    continue
                out.append(f"- frame {e['f']}: slot {r[0]} sends out {sp(r[1])}, HP {r[2]}, moves: "
                           + ", ".join(mv(m) for m in r[4] if m))
        elif e["ev"] == "move_used":
            out.append(f"- frame {e['f']}: {sp(e['species'])} (slot {e['slot']}) used {mv(e['move'])} (PP left {e['pp']})")
        elif e["ev"] == "hp":
            out.append(f"- frame {e['f']}: {sp(e['species'])} HP {e['was']} -> {e['now']}"
                       + (" (fainted)" if e["now"] == 0 else ""))
        elif e["ev"] == "battle_msg":
            text = names["messages"].get(e["id"])
            if text:   # <00> is the game's line break; <XX> placeholders are filled from the names logged with it
                filled = re.sub(r"<([0-9A-F]{2})>", lambda m: e.get("names", {}).get(m.group(1), m.group(0)),
                                text.replace("<00>", " ").replace("<02>", "").replace("<03>", ""))
                nm = e.get("names", {})
                if "<28>" in text and nm.get("DEF") and nm.get("DEF") != nm.get("ATK"):
                    filled += f" (target: {nm['DEF']})"   # a "used <move>" line: the floor's receiving Pokémon
                out.append(f"  - says: {filled}  (message {e['id']:#x})")
        elif e["ev"] == "battle_result":
            out.append(f"- frame {e['f']}: result {e['value']}")
            cur = None
    return out


PC_NAMES = [("A", 0x0100), ("B", 0x0200), ("X", 0x0400), ("Y", 0x0800), ("Z", 0x0010), ("L", 0x0040),
            ("R", 0x0020), ("START", 0x1000), ("UP", 0x0008), ("DOWN", 0x0004), ("LEFT", 0x0001), ("RIGHT", 0x0002)]


def pc_step(raw):
    """One pad-script value (port/src/pc_pad_script.c, as walk_to_slice.py writes it) for 10 latched PADStatus bytes:
    buttons by name, the main stick as SX= / SY= in the raw controller convention (the latched Y is the game's flipped
    copy: flipped back). The C-stick and triggers have no pad-script form; Colosseum's field and menus don't use them."""
    b = (raw[0] << 8) | raw[1]
    toks = [n for n, m in PC_NAMES if b & m]
    sx = ((raw[2] - 256 if raw[2] > 127 else raw[2]) + 128) & 255
    sy = (256 - (((raw[3] - 256 if raw[3] > 127 else raw[3]) + 128) & 255)) & 255
    if sx != 128:
        toks.append(f"SX={sx}")
    if sy != 128:
        toks.append(f"SY={sy}")
    return "+".join(toks) if toks else "-"


def cmd_topc(a):
    """The sitting's presses as a PC pad script, keyed to room entries (load times differ between Dolphin and the
    PC): the first press after an entry is "@0xR+N" (N frames after room R is entered), the others "+N"; presses before
    the first entry keep their absolute frame."""
    sit = find_sitting(a.root, a.n)
    if os.path.exists(os.path.join(sit, "sitting.dtm")):
        # the movie is exact (every poll); pad.jsonl is not (the game polls twice per frame): dtm_to_pad.py
        here = os.path.dirname(os.path.abspath(sys.argv[0]))
        subprocess.run([sys.executable, "-I", os.path.join(here, "dtm_to_pad.py"), sit, a.out]
                       + (["--reseed"] if getattr(a, "reseed", False) else []), check=True)
        return
    print("no Dolphin movie in this sitting: converting the per-frame pad log (not exact)")
    pads = [json.loads(l) for l in open(os.path.join(sit, "pad.jsonl"))]
    evs = [e for e in map(json.loads, open(os.path.join(sit, "events.jsonl"))) if e["ev"] == "entered"]
    entries = [(e["room"], e["f"]) for e in evs]
    seeds = {(e["room"], e["f"]): e.get("rng") for e in evs}
    seeded = set()

    def reseed_upto(f):
        # --reseed (2026-10-19; as dtm_to_pad.py --reseed): at every room entry up to frame f, Dolphin's RNG state
        # there as a "!seed" step (the PC needs ORRE_DEBUG=1); "@R+0" waits for that entry, and the next "@R+N" press
        # counts from the same entry
        for r, ef in entries:
            if ef <= f and (r, ef) not in seeded and seeds.get((r, ef)) is not None:
                lines.append(f"@0x{r:X}+0:!seed 0x{seeds[(r, ef)]:08X}")
                seeded.add((r, ef))
    last = json.load(open(os.path.join(sit, "sitting.json")))["last_frame"]
    lines = [f"# {os.path.basename(sit)} as a PC pad script (playrec.py topc): {len(pads)} pad changes, "
             f"{len(entries)} room entries"]
    prev, used, last_w, absolute = 0, None, None, 0
    for p in pads:
        f = p["f"]
        w = pc_step(bytes.fromhex(p["p"]))
        if w == last_w:
            continue
        ent = [(r, ef) for r, ef in entries if prev < ef <= f]
        if getattr(a, "reseed", False):
            reseed_upto(f)
        if ent and ent[-1] != used and ent[-1] in seeded:
            # the entry already has its "@R+0:!seed" step: count this press from it. A second "@R" would wait for the
            # NEXT entry of R (2026-10-19, verified: the PC idled into the attract demo waiting for a second visit
            # to the title). "@R+0" fires at entry + 1 (the entry happens mid-frame, after that frame's step check), so
            # N - 1 (never below 0) lands on the recording's entry + N, as dtm_to_pad.py --reseed does
            r, ef = ent[-1]
            lines.append(f"+{max(0, f - ef - 1)}:{w}")
            used = ent[-1]
        elif ent and ent[-1] != used:
            r, ef = ent[-1]
            lines.append(f"@0x{r:X}+{f - ef}:{w}")
            used = ent[-1]
        elif used is None:
            lines.append(f"{f}:{w}")
            absolute += 1
        else:
            lines.append(f"+{f - prev}:{w}")
        prev, last_w = f, w
    if getattr(a, "reseed", False):
        reseed_upto(last)
    lines.append(f"# the sitting ended at frame {last} ({last - prev} frames after the last press)")
    open(a.out, "w").write("\n".join(lines) + "\n")
    print(f"{a.out}: {len(lines) - 2} steps ({absolute} before the first room entry, at absolute frames)")


def main():
    # the plan's text has arrows and accented names; a Windows console's default code page can't print them
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--game", choices=("colo", "xd"), default="colo",
                   help="xd: XD's sittings (its disc, _playthrough_xd and dolphin_xdrec.py); default Colosseum")
    p.add_argument("--root", help=f"default {DEFAULT_ROOT} ({XD_ROOT} with --game xd)")
    p.add_argument("--user", help="Dolphin settings folder (default ROOT/dolphin-user)")
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("setup"); s.add_argument("--keep-card", action="store_true")
    sub.add_parser("settings")
    s = sub.add_parser("start"); s.add_argument("--state"); s.add_argument("--note")
    s.add_argument("--section", required=True,
                   help="the playthrough section this sitting plays (S01-S16, BM for battle mode, TEST)")
    s.add_argument("--drive", help=argparse.SUPPRESS)   # test only: a pad.jsonl the script presses while recording
    s = sub.add_parser("after", help="the one-command chain: the Dolphin replay, its check, the PC replay and the "
                                     "report, then '<code>: passes' or the first difference (guide part 11)")
    s.add_argument("n")
    s.add_argument("--probes", default=AFTER_PROBES, help=f"the Dolphin replay's probes (default {AFTER_PROBES})")
    s.add_argument("--check-args", default="", help="more replay_check.py options, one quoted string")
    s = sub.add_parser("replay"); s.add_argument("n"); s.add_argument("--speed", type=float, default=0)
    s.add_argument("--shift", type=int, default=1)
    s.add_argument("--pad", action="store_true", help="replay pad.jsonl even when the sitting has a Dolphin movie")
    s.add_argument("--livepad", help="no movie: feed this livepad_steps.py file live (the PC's presses timed from the "
                                     "events Dolphin shows)")
    s.add_argument("--movie", help="play this .dtm instead of the sitting's own (a converted PC recording, "
                                   "orrepad_to_dtm.py); its path goes to the replay folder's movie.txt")
    s.add_argument("--probe", help="extras logged during a movie replay: camera,rng (camera.csv, rng.csv per frame), "
                                   "frames (frames.csv), dvd (dvd.csv: every disc read's frames, offset and length, "
                                   "for the PC's ORRE_DVD_TRACE), card (card.csv: every memory-card operation's frames, "
                                   "callbacks, EXI commands and addresses), battlemode (battlemode.csv: Battle Mode's "
                                   "kind, battle type and rule whenever they, the room or the battle change)")
    s = sub.add_parser("check"); s.add_argument("n"); s.add_argument("--replay", type=int)
    s = sub.add_parser("report"); s.add_argument("n")
    s = sub.add_parser("topc"); s.add_argument("n"); s.add_argument("out")
    s.add_argument("--reseed", action="store_true", help="Dolphin's RNG state at every room entry ('!seed' steps)")
    s = sub.add_parser("next", help="run 2: the next sitting from the guide's plan, its pre-flight, then start")
    s.add_argument("--code", help="a plan code instead of the next one (P05, P17a, BM1, EVO, SYS)")
    s.add_argument("--note", help="words after the code in the sitting's note")
    s.add_argument("--dry", action="store_true", help="show the plan line and the pre-flight; don't start Dolphin")
    s.add_argument("--bm", action="store_true", help="the next battle-mode sitting (BM1-BM5) instead of the story's")
    sub.add_parser("setup-bm", help="make the battle-mode profile: a copy of the recording profile, empty card")
    # the file name stays "OrrePort next sitting.cmd" although it runs `free` by default (2026-10-26): snatcherLeo's Desktop
    # shortcut has that name, and a new name would leave a stale second launcher behind
    s = sub.add_parser("shortcut", help="put a double-click launcher on the Desktop: `free` by default, `next` with "
                                        "--plan, the battle-mode sitting with --bm")
    s.add_argument("--path", help="where to write it (default: Desktop\\OrrePort next sitting.cmd, or "
                                  "OrrePort battle mode sitting.cmd with --bm)")
    s.add_argument("--bm", action="store_true", help="the battle-mode launcher (Desktop\\OrrePort battle mode sitting.cmd)")
    s.add_argument("--plan", action="store_true", help="the plan-driven launcher (`next`) instead of `free`")
    s = sub.add_parser("free", help="a sitting with no plan code: the pre-flight, then start (the check-out says what "
                                    "it did)")
    s.add_argument("--note", help="optional words in the sitting's note")
    s.add_argument("--dry", action="store_true", help="show the pre-flight; don't start Dolphin")
    s.add_argument("--bm", action="store_true", help="on the battle-mode profile and card, filed under BM")
    a = p.parse_args()
    global GAME, ISO
    GAME = a.game
    if GAME == "xd":
        ISO = XD_ISO
        if a.cmd in ("next", "free", "shortcut", "after", "setup-bm", "check", "report", "topc"):
            # these read Colosseum's plan, events or addresses
            sys.exit(f"`{a.cmd}` is Colosseum-only; for XD: setup, settings, start --section XD, replay N")
    a.root = a.root or (XD_ROOT if GAME == "xd" else DEFAULT_ROOT)
    # absolute paths (2026-11-06): Dolphin runs from its own folder, so a relative --root / --user sent it the
    # wrong user folder and movie; it sat on the title screen with no frame hook (replay 28, P11 note 1)
    a.root = os.path.abspath(a.root)
    if a.user:
        a.user = os.path.abspath(a.user)
    if a.cmd == "setup":
        cmd_setup(a)
    elif a.cmd == "settings":                         # Dolphin on snatcherLeo's folder, no game, no recorder
        subprocess.call([DOLPHIN, "-u", a.user or os.path.join(a.root, "dolphin-user")], cwd=DOLPHIN_DIR)
    elif a.cmd == "start":
        cmd_start(a)
    elif a.cmd == "after":
        sys.exit(cmd_after(a))
    elif a.cmd == "replay":
        cmd_replay(a)
    elif a.cmd == "check":
        cmd_check(a)
    elif a.cmd == "topc":
        cmd_topc(a)
    elif a.cmd == "next":
        cmd_next(a)
    elif a.cmd == "free":
        cmd_free(a)
    elif a.cmd == "shortcut":
        cmd_shortcut(a)
    elif a.cmd == "setup-bm":
        cmd_setup_bm(a)
    else:
        report(find_sitting(a.root, a.n))


if __name__ == "__main__":
    main()
