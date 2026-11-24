"""The save-check sweep's reload checker (the project's helper, 2026-11-20; the project notes, "The save-check
sweep"). Why: OrrePort can save anywhere (PC MENU) and autosaves, but the game only ever saved at a PC, where the room is
rebuilt from the story flags; a save taken elsewhere can hold a room state the room's setup can't rebuild (Rui's sack
hung after Continue). A replay run with ORRE_SAVECHECK=<folder> (port/src/pc_savecheck.cpp) writes a real save at every
moment a player could have saved; this script loads each one the way the player would, in a fresh run, and checks it.

For one save (NNNN.gci + NNNN.ini from the replay's saves folder) it makes a fresh run folder (the exe, DLLs and disc
hard-linked, never copied over anything; Slot 19 holds the test save), boots through tools/local/saferun.py --no-lock with
tools/local/testing/run3_runner.py (its stall check: no new frame for 60 s) and a time cap, and reads the run's log:
  - load: "auto" = the autosave path (Slot 19's own save is a template, the test save is Slot 19\\Autosave\\autosave.gci
    with its .ini, ORRE_AUTOSAVE_LOAD=19), "pcmenu" = PC MENU's path (the test save is the slot's own .gci, slot.ini's
    spot= line names it); --rollback "993=0 ..." sets those flags with the debug command at the main menu before
    CONTINUE, the same image a PC MENU save written with the open-scene table's rollback gives (likely; the softlock
    notes' rollback test did exactly this);
  - control: the probe's "control at frame" line within --control-frames (default 600, 10 s) of Continue, and Wes moving
    when the replayed inputs push the stick;
  - no hold over 20 s: the probe's "HELD 20 s" line or the hang watch's own line is a FAIL;
  - the people: each person the game had at that moment (the .ini's npc= rows) against the reload's (the probe's rows):
    shown / hidden, standing vs wandering, and a standing person's place within --step units;
  - the story: the player's own next inputs from the recording (--inputs frames after the save's frame, default 1800,
    30 s) replayed from the room's fade-in, and the story flags they set compared with the replay's (drift is possible:
    people wander differently after a reload, so a flag difference is a CHECK, not a FAIL);
  - extra pad steps (--pad-extra "+60:!talk 111 9" ...) for a hand-made next step, as the softlock notes' section 3 did.
Each check gives PASS / CHECK / FAIL with the reason; --table appends one row to a tab-separated file.

  python -I tools/local/testing/savecheck.py <replay run dir> <save number> [--load auto|pcmenu] [--rollback "993=0"]
         [--inputs N] [--pad-extra STEP ...] [--tail N] [--name NAME] [--table FILE] [--template GCI] [--exe-dir DIR]

Folders come from the environment, with defaults that need no one PC's paths (2026-11-24, for the public repo):
SAVECHECK_SCRATCH (the run folders; default: orre_savecheck in the system's temp folder), SAVECHECK_ISO (the disc to
hard-link; default: the repo's orig/ image; NTFS allows 1,023 links per file, so spare copies are made beside the run
folders when it's full), RUN3_DIR (the recorded sittings' own folder with the runtime DLLs; default:
Desktop/OrrePort/Final/Run3 in the home folder). Windows only as written (the exe name, MSYS2's PATH line); on Linux /
macOS those would change, the logic would not (likely).
"""
import argparse, hashlib, os, re, shutil, subprocess, sys, tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SCRATCH = os.environ.get("SAVECHECK_SCRATCH", os.path.join(tempfile.gettempdir(), "orre_savecheck"))
ISO = os.environ.get("SAVECHECK_ISO", os.path.join(REPO, "orig", "Pokemon Colosseum (USA).iso"))
RUN3 = os.environ.get("RUN3_DIR", os.path.join(os.path.expanduser("~"), "Desktop", "OrrePort", "Final", "Run3"))
TITLE = ["2:-", "710:A", "747:-", "760:A", "814:-", "824:A", "857:-", "899:A", "912:-", "988:A", "996:-", "1032:A", "1041:-"]
# the title's presses to CONTINUE and YES (ORRE_PLAYER_DEFAULTS: YES first), from the autosave tests (load1.pad); a
# rollback's debug commands go at 975, after the card read and before CONTINUE (the softlock notes' rollback test)


def read_ini(path):
    out, npc = {}, []
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\r\n")
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        if k == "npc":
            npc.append(v)
        elif k not in out:
            out[k] = v
    out["npc"] = npc
    return out


def people(rows):
    """{(group, index): (shown, state, x, z)}; the player (0/100) and Rui (0/101) left out: they follow the spot"""
    d = {}
    for r in rows:
        p = r.split()
        if len(p) < 9:
            continue
        idx, grp = int(p[1]), int(p[2])
        if grp == 0 and idx in (100, 101):
            continue
        x = None if p[6] == "-" else float(p[6])
        z = None if p[8] == "-" else float(p[8])
        d[(grp, idx)] = (int(p[3]), int(p[4]), x, z)
    return d


def compare_people(orig, now, step):
    diffs = []
    for key, (shown, state, x, z) in sorted(orig.items()):
        if key not in now:
            if shown:
                diffs.append("%d/%d shown but missing after reload" % key)
            continue
        s2, st2, x2, z2 = now[key]
        if shown != s2:
            diffs.append("%d/%d %s -> %s" % (key + ("shown" if shown else "hidden", "shown" if s2 else "hidden")))
            continue
        if not shown:
            continue
        if (state == 4) != (st2 == 4):
            diffs.append("%d/%d %s -> %s" % (key + ("wandering" if state == 4 else "state %d" % state,
                                                    "wandering" if st2 == 4 else "state %d" % st2)))
        # a place only for people standing (state 0) both times: other states are walks in progress (3: a scripted
        # walk, likely), whose place depends on the frame
        elif state == 0 and st2 == 0 and x is not None and x2 is not None and ((x - x2) ** 2 + (z - z2) ** 2) ** 0.5 > step:
            diffs.append("%d/%d at (%.0f, %.0f) -> (%.0f, %.0f)" % (key + (x, z, x2, z2)))
    for key, (s2, st2, x2, z2) in sorted(now.items()):
        if key not in orig and s2:
            diffs.append("%d/%d new and shown after reload" % key)
    return diffs


def pad_steps(pad_path):
    steps = []
    for line in open(pad_path, encoding="utf-8"):
        if line[:1].isdigit():
            f, what = line.rstrip("\n").split(":", 1)
            steps.append((int(f), what))
    return steps


def flags_after(log_path, start, end=None):
    """[(frame, id, new)] story-flag changes in a log from frame `start` (exclusive) to `end` (inclusive)"""
    out = []
    pat = re.compile(r"^\[flag\] (\d+): (\d+) -> (\d+) \(frame (\d+)")
    for line in open(log_path, encoding="utf-8", errors="replace"):
        m = pat.match(line)
        if m:
            fr = int(m.group(4))
            if fr > start and (end is None or fr <= end):
                out.append((fr, int(m.group(1)), int(m.group(3))))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("replay")
    ap.add_argument("save", type=int)
    ap.add_argument("--load", choices=["auto", "pcmenu"], default="auto")
    ap.add_argument("--rollback", default="")
    ap.add_argument("--inputs", type=int, default=1800)
    ap.add_argument("--pad-extra", nargs="*", default=[])
    ap.add_argument("--tail", type=int, default=1500, help="frames after the replayed inputs (the 20 s hang watch fits)")
    ap.add_argument("--control-frames", type=int, default=600)
    ap.add_argument("--step", type=float, default=20.0)
    ap.add_argument("--name", default="")
    ap.add_argument("--table", default="")
    ap.add_argument("--template", default="")
    ap.add_argument("--exe-dir", default=os.path.join(SCRATCH, "exe1"))
    ap.add_argument("--expect", default="", help="flags the next step must set, e.g. '996=1 997=1' (else FAIL)")
    ap.add_argument("--time", type=int, default=420)
    ap.add_argument("--recheck", action="store_true", help="judge an earlier reload (same --name) again; no run")
    a = ap.parse_args()

    rep = os.path.abspath(a.replay)
    base = os.path.join(rep, "saves", "%04d" % a.save)
    ini = read_ini(base + ".ini")
    room = int(ini["room"], 16)
    frame = int(ini["frame"])
    name = a.name or "%s_%04d_%s%s" % (os.path.basename(rep), a.save, a.load, "_rb" if a.rollback else "")
    out = os.path.join(SCRATCH, "reload", name)
    if a.recheck:
        # *Added 2026-11-20:* judge an earlier reload again with the current rules (its run.log), without running it
        if not os.path.exists(os.path.join(out, "run.log")):
            sys.exit(out + " has no run.log to recheck")
    elif os.path.exists(out) and os.path.exists(os.path.join(out, "run.log")):
        sys.exit(out + " exists: give a new --name")
    else:
        if os.path.exists(out):
            # a folder left half-made by a run that couldn't start (no run.log; never deleted): a fresh one beside it
            # ("_retry", "_retry2", ...); one that has a run.log means the save was done
            base_name, k = name, 1
            while os.path.exists(out):
                if os.path.exists(os.path.join(out, "run.log")):
                    sys.exit(out + " exists: give a new --name")
                name = base_name + ("_retry" if k == 1 else "_retry%d" % k)
                out = os.path.join(SCRATCH, "reload", name)
                k += 1
        prepare(a, rep, base, ini, out)
    stop, nxt = pad_script(a, rep, frame, room, out)
    r = run(a, rep, out, name, stop) if not a.recheck else None
    log = os.path.join(out, "run.log")
    text = open(log, encoding="utf-8", errors="replace").read() if os.path.exists(log) else ""
    judge(a, rep, ini, room, frame, name, out, nxt, r, log, text)


def link_disc(dest):
    """the disc as a hard link (a run never copies 1.4 GB). *Added 2026-11-20:* NTFS allows at most 1,023 links to one
    file and a sweep makes hundreds of run folders, so when the main image is full the first spare copy under
    <scratch>/discs/ with room is used, and a new spare copy is made when none has room (as run3_pc_replay.sh does).
    Windows / NTFS only in practice; ext4 and APFS allow far more links (likely)"""
    import glob
    spares = sorted(glob.glob(os.path.join(SCRATCH, "discs", "disc_*.iso")))
    for src in [ISO] + spares:
        try:
            os.link(src, dest)
            return
        except OSError:
            continue
    os.makedirs(os.path.join(SCRATCH, "discs"), exist_ok=True)
    new = os.path.join(SCRATCH, "discs", "disc_%d.iso" % len(spares))
    shutil.copyfile(ISO, new)
    os.link(new, dest)


def link_or_copy(src, dst):
    """a hard link, or a copy once the file has NTFS's 1,023 links (the exe copy is linked into every run folder)"""
    try:
        os.link(src, dst)
    except OSError:
        shutil.copy2(src, dst)


def prepare(a, rep, base, ini, out):
    """the fresh run folder: hard links to the exe copy's files and the disc, Slot 19 with the test save"""
    slot = os.path.join(out, "Colosseum", "Saves", "Slot 19")
    os.makedirs(os.path.join(out, "Colosseum", "ROM"))
    os.makedirs(os.path.join(out, "user"))
    os.makedirs(slot)
    # the exe, DLLs and fonts: hard links to the batch's own copy (never build-pc, which is relinked in place)
    for f in os.listdir(a.exe_dir):
        p = os.path.join(a.exe_dir, f)
        if f.lower().endswith((".exe", ".dll")):
            link_or_copy(p, os.path.join(out, f))
    for f in ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"):
        if not os.path.exists(os.path.join(out, f)):
            shutil.copy(os.path.join(RUN3, f), out)
    shutil.copytree(os.path.join(a.exe_dir, "Fonts"), os.path.join(out, "Fonts"), copy_function=link_or_copy)
    shutil.copytree(os.path.join(a.exe_dir, "ui"), os.path.join(out, "ui"), copy_function=link_or_copy)
    link_disc(os.path.join(out, "Colosseum", "ROM", "Pokemon Colosseum (USA).iso"))
    open(os.path.join(out, "user", "settings.ini"), "w").write(
        "# save-check reload (scratch): Slot 19, normal mode\nsave_slot = 19\nlanguage = en\naccurate_new_saves = 0\n")
    slot_ini = "# save-check reload (scratch)\nversion=1\nname=savecheck\ncame_from=imported\nnew=0\norder=1\ngamecube_accurate=0\n"
    gci = open(base + ".gci", "rb").read()
    if a.load == "auto":
        templ = a.template or os.path.join(rep, "Colosseum", "Saves", "Slot 1", "01-GC6E-pokemon_colosseum.gci")
        shutil.copy(templ, os.path.join(slot, "01-GC6E-pokemon_colosseum.gci"))
        os.makedirs(os.path.join(slot, "Autosave"))
        shutil.copy(base + ".gci", os.path.join(slot, "Autosave", "autosave.gci"))
        shutil.copy(base + ".ini", os.path.join(slot, "Autosave", "autosave.ini"))
    else:
        open(os.path.join(slot, "01-GC6E-pokemon_colosseum.gci"), "wb").write(gci)
        spot = ini.get("spot", "").split(" ", 1)
        keep = " spot=pc" not in (ini.get("window", "") + " ")
        if len(spot) == 2 and keep:
            slot_ini += "spot=%s %s\n" % (hashlib.sha1(gci).hexdigest(), spot[1])
    open(os.path.join(slot, "slot.ini"), "w").write(slot_ini)


def pad_script(a, rep, frame, room, out):
    """the pad: the title to CONTINUE (+ the rollback), then the player's own next inputs from the room's fade-in.
    Returns the stop frame and the replayed inputs; written only for a new run"""
    steps = list(TITLE)
    for i, tok in enumerate(a.rollback.split()):
        steps.insert(9 + i, "%d:!flag %s" % (975 + i, tok.replace("=", " ")))
    steps.sort(key=lambda s: int(s.split(":")[0]))
    rec = pad_steps(os.path.join(rep, "sitting.pad"))
    held = "-"
    for f, what in rec:
        if f <= frame:
            held = what
    nxt = [(f, w) for f, w in rec if frame < f <= frame + a.inputs]
    # the first step waits for the room's fade-in (anchor 0x3<room>), with a fallback so a missing anchor can't stall it
    steps.append("@0x%X+30~1200:%s" % (0x30000 | room, held if not held.startswith("!") else "-"))
    prev = frame
    first = True
    for f, w in nxt:
        if w.startswith("!"):
            continue
        steps.append("+%d:%s" % (max(1, f - prev) if not first else max(1, f - frame), w))
        prev, first = f, False
    steps.append("+1:-")
    for e in a.pad_extra:
        steps.append(e)
    steps.append("+%d:-" % (a.tail + 4000))  # keeps the script running past the stop (an ended script paces real time)
    if not a.recheck:
        open(os.path.join(out, "sitting.pad"), "w").write(
            "# save-check reload of %s save %04d (%s)\n%s\n" % (os.path.basename(rep), a.save, a.load, "\n".join(steps)))
    stop = 1041 + 1200 + a.inputs + a.tail + sum(int(re.match(r"\+(\d+)", e).group(1)) for e in a.pad_extra if e.startswith("+"))
    return stop, nxt


def run(a, rep, out, name, stop):
    """the run itself: saferun --no-lock around run3_runner.py (its stall check), with the probe and the logs on"""
    env = dict(os.environ)
    env["PATH"] = r"C:\msys64\mingw64\bin;" + env.get("PATH", "")
    padpath = os.path.join(out, "sitting.pad").replace("\\", "/")
    env.update(ORRE_WINDOW="minimized", ORRE_REPLAY_SPEED="max", ORRE_PAD_SCRIPT="@file:" + padpath, ORRE_EVENT_LOG="1",
               ORRE_DEBUG="1", ORRE_PLAYER_DEFAULTS="1", ORRE_FIELD_PC_MENU="1", ORRE_AUTOSAVE="1", ORRE_STUCK_WATCH="1",
               ORRE_SAVECHECK_PROBE="1", ORRE_RUN_WHY="save-check reload " + name, RUN3_STOP=str(stop))
    if a.load == "auto":
        env["ORRE_AUTOSAVE_LOAD"] = "19"
    env.pop("ORRE_SAVECHECK", None)
    cmd = [sys.executable, "-I", os.path.join(REPO, "tools", "local", "saferun.py"), "--no-lock", "--time", str(a.time),
           "--why", "Save-check sweep: reload test save %04d of %s (%s)" % (a.save, os.path.basename(rep), a.load), "--",
           sys.executable, "-I", os.path.join(REPO, "tools", "local", "testing", "run3_runner.py"), out]
    return subprocess.run(cmd, cwd=out, env=env, capture_output=True, text=True)


def judge(a, rep, ini, room, frame, name, out, nxt, r, log, text):
    """the verdict from the run's log: PASS / CHECK / FAIL with the reasons; one table row"""
    verdict, why = "PASS", []
    def bad(level, msg):
        nonlocal verdict
        why.append(msg)
        if level == "FAIL" or verdict == "PASS":
            verdict = level if verdict != "FAIL" else "FAIL"
    m = re.search(r"\[savecheck\] probe: Continue at frame (\d+) goes to (the spot: room 0x([0-9A-F]+)|the save's own return point)", text)
    cont = int(m.group(1)) if m else None
    if not m:
        bad("FAIL", "no Continue (run %s)" % (r.returncode if r is not None else "rechecked"))
    elif m.group(3) is None or int(m.group(3), 16) != room:
        bad("CHECK", "Continue went to %s, not room 0x%X" % (m.group(2), room))
    c = re.search(r"\[savecheck\] probe: control at frame (\d+) \((\d+) frames after Continue\) in room 0x([0-9A-F]+)", text)
    ctl = int(c.group(1)) if c else None
    # the story: flags the replay set in the same stretch of inputs vs the reload's (from control, or from Continue
    # when control never came)
    orig = flags_after(os.path.join(rep, "run.log"), frame, frame + a.inputs)
    after = flags_after(log, ctl or cont or 10 ** 9)
    o = {(i, v) for _, i, v in orig}
    n = {(i, v) for _, i, v in after}
    if cont is not None and not c:
        # *Changed 2026-11-20 (P06 save 31):* a scene that starts the moment the room is back (no two free frames) and
        # plays on as in the replay is the game's own continuation, not a stuck save: a CHECK when the reload set every
        # flag the replay set in that stretch, a FAIL otherwise
        battle = re.search(r"^\[battle\] start (\d+)", text[text.find("probe: Continue at frame"):], re.M)
        if o and not (o - n):
            bad("CHECK", "no free frame after Continue: a scene started at once and set the replay's flags")
        elif battle:
            # *Added 2026-11-20 (P14, Snagem Hideout):* a trainer's battle that starts the moment the room is back (the
            # room's own entry: a trainer who sees the player on entering) is the game's own, not a stuck save
            bad("CHECK", "no free frame after Continue: a battle (%s) started on entering" % battle.group(1))
        else:
            bad("FAIL", "no control after Continue")
    elif c and int(c.group(2)) > a.control_frames:
        bad("FAIL", "control only %s frames after Continue" % c.group(2))
    # *Changed 2026-11-20 (P16, the Relic Stone):* a hold that later ends by itself is a long scene, not a hang (the
    # purification ceremony runs in a pushed room for about 21 s, and the probe's 20 s counted across it): a CHECK. A
    # FAIL is a hold that never ends, or the hang watch's own line
    h = re.search(r"probe: HELD 20 s from frame (\d+) with nothing on screen to answer \(([^,]+)", text)
    # what happens after the hang watch has offered its way back (its question, the main menu) is the watch's doing,
    # not the scene's: the search for an end stops there (Rui's stuck sack, whose watch offer leads to the main menu)
    offer = text.find("hang watch: asking", h.end() if h else 0)
    upto = offer if offer >= 0 else len(text)
    ended = h is not None and re.search(r"probe: free again at frame \d+", text[h.end():upto]) is not None
    # *Changed 2026-11-20 (P13 save 29, Realgam Tower):* a hold during which the story visibly goes on (a room change,
    # a new line of text, a battle) is a long scene, also when the run ends before control comes back (the tower's walk
    # to NASCOUR, his speech and the battle after it, more than a minute with no control in the replay too). A hang
    # shows none of them (Rui's sack: the same frame for 50 s). Not story flags: some count on their own (209 rose every
    # 10 frames in Realgam Tower)
    if h is not None and not ended:
        start = text.rfind("probe: held from frame", 0, h.start())  # where this hold began (the last one before it)
        start = start if start >= 0 else h.start()
        # rooms and battles only: a [text] line is printed when its message has been shown, so the line that starts a
        # hang (Rui's woman: "Please! We need your help!") lands just after the hold begins
        ended = re.search(r"^\[(room\] request|battle\] start)", text[max(start, 0):upto], re.M) is not None
    # *Changed 2026-11-20 (P12, The Under's subway):* the hang watch's own 20 s line counts as a FAIL only when the hold
    # didn't end: the subway ride is a touch script that holds the player about 22 s with nothing on screen and then
    # lets go (a CHECK, and a finding for the watch's limit)
    watch = "hang watch: a talk / touch script has held the player" in text
    # *Changed 2026-11-20 (P11 save 135, The Under):* a "hold" whose reason by then is only "not a resumable field spot"
    # is the player free in a story area with its own return point pending (PC MENU's spot rule; Pyrite's lying
    # scientist showed the same): no hang
    if h and not ended and re.search(r"now: not a resumable field spot\)", text[h.start():h.start() + 400]):
        ended = True
    if (watch and not (h and ended)) or (h and not ended):
        bad("FAIL", "held over 20 s" + (" from frame %s (%s)" % (h.group(1), h.group(2)) if h else " (hang watch)"))
    elif h:
        bad("CHECK", "a hold of 20 s or more that ended or went on (rooms, text, battles), from frame %s (%s)" %
            (h.group(1), h.group(2)))
    # did Wes move when the inputs pushed the stick?
    moves = [(int(x.group(1)), float(x.group(2)), float(x.group(4))) for x in
             re.finditer(r"probe: wes frame (\d+) room 0x[0-9A-F]+ (\S+) (\S+) (\S+)", text)]
    stick = any("SX=" in w or "SY=" in w for _, w in nxt)
    moved = len({(round(x, 1), round(z, 1)) for _, x, z in moves}) > 1
    # a FAIL only when nothing else had the stick: the player's own inputs may open a menu (the P*DA on Y, P01 save 34)
    # or start a scene, and then the stick moves a cursor, not Wes (*changed 2026-11-20*: that case was a false FAIL)
    other = re.search(r"probe: held from frame", text) is not None
    if ctl is not None and stick and not moved:
        bad("CHECK" if other else "FAIL", "Wes never moved under the replayed stick inputs" +
            (" (a menu or scene had them)" if other else ""))
    # the people
    now = [ln.split("probe npc ", 1)[1] for ln in text.splitlines() if "[savecheck] probe npc " in ln]
    pd = compare_people(people(ini["npc"]), people(now), a.step) if now else ["no people rows after reload"]
    if pd:
        bad("CHECK", "people: " + "; ".join(pd[:6]) + (" (+%d)" % (len(pd) - 6) if len(pd) > 6 else ""))
    if o - n:
        bad("CHECK", "flags the replay set that the reload didn't: " + " ".join("%d=%d" % x for x in sorted(o - n)))
    for tok in a.expect.split():
        fid, val = (int(x) for x in tok.split("="))
        if (fid, val) not in n:
            bad("FAIL", "expected flag %d=%d not set" % (fid, val))
    texts = [t for t in re.findall(r"^\[text\] (.*?)   \(message", text, re.M)]
    gives = [t for t in texts if re.search(r"obtained|received|got the|put away|was registered|became a partner|found \d", t, re.I)]
    battles = len(re.findall(r"^\[battle\] start", text, re.M))
    row = [name, os.path.basename(rep), "%04d" % a.save, "%d" % frame, "0x%X" % room, ini.get("why", ""), a.load,
           a.rollback or "-", verdict, " | ".join(why) or "-", "ctl+%s" % (c.group(2) if c else "?"),
           " ".join("%d=%d" % (i, v) for _, i, v in after[:20]) or "-", " / ".join(gives[:4]) or "-", str(battles)]
    print("\t".join(row))
    if a.table:
        new = not os.path.exists(a.table)
        with open(a.table, "a", encoding="utf-8") as t:
            if new:
                t.write("\t".join(["run", "replay", "save", "frame", "room", "why", "load", "rollback", "verdict", "reasons",
                                   "control", "flags set after reload", "items/given lines", "battles"]) + "\n")
            t.write("\t".join(row) + "\n")


if __name__ == "__main__":
    main()
