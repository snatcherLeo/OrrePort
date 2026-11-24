"""Run one heavy command (a build or a PC boot / test run) safely, one at a time across sessions (snatcherLeo, 2026-10-16).

After a night where the PC stopped responding while several sessions built and booted at once, every heavy run goes
through this wrapper:
  - four at a time (snatcherLeo, 2026-10-19; two since 2026-10-17, one before): four slots in `../.orre_heavy.lock` (the
    folder above the checkouts, so every lane shares it); a fifth run waits, and at most two of the four may be builds
    (build tokens). **Test runs use their own copy of the exe** (2026-10-19), so a build never waits for or breaks
    a run. **Exclusive runs** (`--exclusive`, automatic for Dolphin; only for runs that depend on real time) run alone: Dolphin comparison runs and timing-sensitive measurements (audio, RNG counts, load stalls). Byte-range
    locks: Windows releases them when the holder dies, so a crash never leaves them stuck;
  - builds at most 12 jobs (`-j` / `--jobs` above 12 are lowered), so two builds share the PC's 32 threads;
  - a time limit (--time, default 300 s) after which the whole process tree is killed, including programs that
    escaped the job object (the Store Python's children: noted while the command runs and ended at its end,
    2026-11-02, after an orphaned Dolphin ran on for two hours);
  - a memory cap (--mem-gb, default 8 GB) for the whole tree (Windows job object; on other systems only the time
    limit and the lock apply);
  - below-normal priority, so the desktop stays usable;
  - ORRE_WATCHDOG (B's stall report, default 15 s) and ORRE_FRAME_LOG set for boot runs unless already set;
  - waits while snatcherLeo is playing (the marker `../.orre_player_playing` written by build-pc/run_game.ps1), so no session
    opens a test window or slows his game; --no-lock (light commands only) skips this.
  - the run board: while it waits or runs, it lists itself (session, command, --why, start time, limit) in
    `../.orre_runs/`; `python -I tools/local/whatsrunning.py` prints what is running and what waits.
Exit code: the command's, 124 on the time limit, 125 if the job could not be set up.
Usage (any checkout):
  python -I tools/local/saferun.py [--time S] [--mem-gb N] [--no-lock] [--log FILE] [--why TEXT] -- COMMAND ARGS...
  e.g. python -I tools/local/saferun.py --time 150 --log run.log -- build-pc/firstlink/orrecolo.exe --disc "<iso>"
"""
import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

LOCK = Path(__file__).resolve().parents[3] / ".orre_heavy.lock"  # decomp/ (above pkmn_colosseum, pc-hsd, pc-platform)
# "snatcherLeo is playing" marker (snatcherLeo, 2026-10-17): written by build-pc/run_game.ps1 with its process id while he plays and
# removed when the game closes. While it names a live process, every heavy run waits, so no session can open a test
# window or slow his game. A marker left behind by a crash names a dead process and is ignored.
# (Renamed 2026-11-24 from a name holding a person's name, so the public copy of this script carries none; every
# reader and the launcher changed in the same commit.)
PLAYING = LOCK.parent / ".orre_player_playing"
# The run board (snatcherLeo, 2026-10-17: "be VERY clear when a build is running, what is running and why"): every saferun
# writes one small file here while it waits or runs, removed when it ends; tools/local/whatsrunning.py shows them.
BOARD = LOCK.parent / ".orre_runs"
LANES = {"pkmn_colosseum": "Main", "pc-hsd": "A", "pc-platform": "B", "docs-c": "C", "formats-d": "D"}


def session_name(cmd=()):
    """Which checkout started this run: the checkout it runs in (the main one or a lane worktree); from the folder above
    the checkouts, the first lane checkout the command's own program or script lives in (not the disc path, which is
    always the main checkout's); else this saferun copy's checkout"""
    lane = lane_of(cmd)  # a lane's own program or script, even when started from the project's checkout
    if lane:
        return lane
    if Path.cwd().name in LANES:
        return LANES[Path.cwd().name]
    return LANES.get(Path(__file__).resolve().parents[2].name) or Path.cwd().name


def lane_of(paths):
    """The session whose checkout the first of these paths lives in (disc images skipped: always in the project's), or None"""
    for c in paths:
        c = c.replace("\\", "/")
        if c.lower().endswith((".iso", ".gcm")):
            continue
        for k, v in LANES.items():
            if f"/{k}/" in c or c.startswith(f"{k}/"):
                return v
    return None


def board_write(state, cmd, why, limit):
    try:
        BOARD.mkdir(exist_ok=True)
        what = " ".join(Path(c).name if i == 0 else c for i, c in enumerate(cmd))[:160]
        (BOARD / f"{os.getpid()}.txt").write_text(
            f"pid={os.getpid()}\nstate={state}\nsession={session_name(cmd)}\nsince={time.strftime('%H:%M:%S')}\n"
            f"limit={limit:.0f}\nwhat={what}\nwhy={why or '(not given)'}\n", encoding="utf-8")
    except OSError:
        pass  # the board is a convenience; never block a run on it


def board_remove():
    try:
        (BOARD / f"{os.getpid()}.txt").unlink()
    except OSError:
        pass


def process_alive(pid):
    if os.name != "nt":
        try:
            os.kill(pid, 0)  # POSIX: signal 0 only checks the process exists
            return True
        except OSError:
            return False
    import ctypes
    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    handle = k32.OpenProcess(0x00100000 | 0x1000, False, pid)  # SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION
    if not handle:
        return False
    try:
        return k32.WaitForSingleObject(handle, 0) == 0x102  # WAIT_TIMEOUT: still running
    finally:
        k32.CloseHandle(handle)


def wait_while_player_plays():
    waited = False
    while PLAYING.exists():
        try:
            pid = int(PLAYING.read_text(errors="replace").split()[0])
        except (OSError, ValueError, IndexError):
            return  # unreadable marker: don't block on it
        if not process_alive(pid):
            return  # left behind by a crash
        if not waited:
            print("[saferun] snatcherLeo is playing the game; waiting until he closes it", file=sys.stderr, flush=True)
            waited = True
        time.sleep(5)
    if waited:
        print("[saferun] the game is closed; continuing", file=sys.stderr, flush=True)


# Four heavy runs at a time (snatcherLeo, 2026-10-19: "Yes, 4 at once"; was two since 2026-10-17), one lock byte each; bytes 0
# and 1 stay the first two slots so a saferun copy from before keeps working alongside. Builds also need one of two
# build tokens, so at most two builds run at once (two builds at 12 jobs share the PC's 32 threads).
SLOTS = (0, 1, 3, 4)
DOOR = 2               # an exclusive run holds this byte while it waits for every slot, so no new run starts meanwhile
BUILD_TOKENS = (5, 6)
BUILD_TOOLS = ("ninja", "firstlink.py", "cmake", "make", "configure.py")


def is_build(cmd):
    """A build command (its program or script is one of BUILD_TOOLS), which also takes a build token"""
    for c in cmd[:3]:
        name = Path(c.replace("\\", "/")).name.lower()
        if name.endswith(".exe"):
            name = name[:-4]
        if name in BUILD_TOOLS:
            return True
    return False


def _try_lock(f, offset):
    import msvcrt
    try:
        f.seek(offset)
        msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
        return True
    except OSError:
        return False


def _unlock(f, offset):
    import msvcrt
    f.seek(offset)
    msvcrt.locking(f.fileno(), msvcrt.LK_UNLCK, 1)


def acquire_lock(exclusive=False, build=False):
    """Four slots (snatcherLeo, 2026-10-19; two before): a normal run (builds, ordinary boots) takes one free slot once the
    door is open, and a build also takes one of the two build tokens first; an exclusive run (Dolphin comparison runs
    and timing-sensitive measurements: audio, RNG counts, load stalls, which must run alone) closes the door, then takes
    every slot. Byte-range locks: Windows releases them when the holder dies. Older saferun copies lock byte 0 only,
    which is slot 0 here, so they stay compatible."""
    f = open(LOCK, "a+b")
    if os.name == "nt":
        waited = False

        def note_wait(what):
            nonlocal waited
            if not waited:
                holder = Path(str(LOCK) + ".who").read_text(errors="replace").strip() if Path(str(LOCK) + ".who").exists() else "?"
                print(f"[saferun] waiting for {what} (last taken by: {holder})", file=sys.stderr, flush=True)
                waited = True

        if exclusive:
            while not _try_lock(f, DOOR):
                note_wait("the door (another exclusive run)")
                time.sleep(2)
            for slot in SLOTS:
                while not _try_lock(f, slot):
                    note_wait("every slot (an exclusive run waits for the running ones to finish)")
                    time.sleep(2)
        else:
            if build:
                while not any(_try_lock(f, t) for t in BUILD_TOKENS):
                    note_wait("a build token (two builds are running)")
                    time.sleep(2)
            while True:
                if _try_lock(f, DOOR):
                    _unlock(f, DOOR)  # the door is open: no exclusive run is waiting or running
                    if any(_try_lock(f, slot) for slot in SLOTS):
                        break
                note_wait("a free slot")
                time.sleep(2)
    else:
        import fcntl
        fcntl.flock(f.fileno(), fcntl.LOCK_EX)  # elsewhere: one at a time, as before
    Path(str(LOCK) + ".who").write_text(f"pid {os.getpid()} in {os.getcwd()} since {time.strftime('%H:%M:%S')}"
                                        f"{' (exclusive)' if exclusive else ''}\n")
    return f


def cap_jobs(cmd, limit=12):
    """Builds run at most 12 jobs (snatcherLeo, 2026-10-17: two builds at once share the PC's 32 threads): rewrites -j N /
    -jN / --jobs N / --jobs=N above the limit, also inside a `bash -c "..."` command string"""
    import re

    def fix(arg):
        return re.sub(r"(-j\s*|--jobs[ =])(\d+)", lambda m: m.group(1) + str(min(int(m.group(2)), limit)), arg)

    out = [fix(a) for a in cmd]
    for i, a in enumerate(out[:-1]):
        if a in ("-j", "--jobs") and out[i + 1].isdigit():
            out[i + 1] = str(min(int(out[i + 1]), limit))
    return out


def windows_job(mem_bytes):
    """A job object that kills every process in it when closed, caps their total committed memory, and runs them at
    below-normal priority."""
    import ctypes
    from ctypes import wintypes
    k32 = ctypes.WinDLL("kernel32", use_last_error=True)

    class IO_COUNTERS(ctypes.Structure):
        _fields_ = [(n, ctypes.c_ulonglong) for n in ("r", "w", "o", "rb", "wb", "ob")]

    class BASIC(ctypes.Structure):
        _fields_ = [("PerProcessUserTimeLimit", ctypes.c_longlong), ("PerJobUserTimeLimit", ctypes.c_longlong),
                    ("LimitFlags", wintypes.DWORD), ("MinimumWorkingSetSize", ctypes.c_size_t),
                    ("MaximumWorkingSetSize", ctypes.c_size_t), ("ActiveProcessLimit", wintypes.DWORD),
                    ("Affinity", ctypes.c_size_t), ("PriorityClass", wintypes.DWORD),
                    ("SchedulingClass", wintypes.DWORD)]

    class EXTENDED(ctypes.Structure):
        _fields_ = [("Basic", BASIC), ("Io", IO_COUNTERS), ("ProcessMemoryLimit", ctypes.c_size_t),
                    ("JobMemoryLimit", ctypes.c_size_t), ("PeakProcessMemoryUsed", ctypes.c_size_t),
                    ("PeakJobMemoryUsed", ctypes.c_size_t)]

    LIMIT_PRIORITY, LIMIT_JOB_MEMORY, LIMIT_KILL_ON_CLOSE = 0x20, 0x200, 0x2000
    k32.CreateJobObjectW.restype = wintypes.HANDLE
    job = k32.CreateJobObjectW(None, None)
    info = EXTENDED()
    info.Basic.LimitFlags = LIMIT_PRIORITY | LIMIT_JOB_MEMORY | LIMIT_KILL_ON_CLOSE
    info.Basic.PriorityClass = 0x4000  # BELOW_NORMAL_PRIORITY_CLASS
    info.JobMemoryLimit = mem_bytes
    if not job or not k32.SetInformationJobObject(job, 9, ctypes.byref(info), ctypes.sizeof(info)):
        raise OSError(ctypes.get_last_error(), "job object")

    def peak():
        k32.QueryInformationJobObject(job, 9, ctypes.byref(info), ctypes.sizeof(info), None)
        return info.PeakJobMemoryUsed

    return k32, job, peak


# Escaped processes (2026-11-02, after the orphaned Dolphin): a native program started by the Microsoft Store
# Python (the WindowsApps alias, a packaged app) lands outside the job object, so TerminateJobObject missed it: the
# Dolphin from a timed-out playrec ran on for two hours with its parent gone (verified with ping.exe the same way:
# both Pythons died, ping lived). The wrapper now notes every process in the command's tree while it runs (start
# time included, so a reused process id is never hit) and ends the ones still alive when the command ends or times out.
def win_process_table(k32):
    """{pid: (parent pid, exe name)} of every process now (one Toolhelp snapshot)."""
    import ctypes
    from ctypes import wintypes

    class PE32(ctypes.Structure):
        _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD), ("th32ProcessID", wintypes.DWORD),
                    ("th32DefaultHeapID", ctypes.c_size_t), ("th32ModuleID", wintypes.DWORD),
                    ("cntThreads", wintypes.DWORD), ("th32ParentProcessID", wintypes.DWORD),
                    ("pcPriClassBase", ctypes.c_long), ("dwFlags", wintypes.DWORD),
                    ("szExeFile", ctypes.c_wchar * 260)]

    k32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    k32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    k32.Process32FirstW.argtypes = k32.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
    k32.CloseHandle.argtypes = [wintypes.HANDLE]
    snap = k32.CreateToolhelp32Snapshot(2, 0)  # TH32CS_SNAPPROCESS
    table = {}
    if not snap or snap == wintypes.HANDLE(-1).value:
        return table
    e = PE32()
    e.dwSize = ctypes.sizeof(PE32)
    ok = k32.Process32FirstW(snap, ctypes.byref(e))
    while ok:
        table[e.th32ProcessID] = (e.th32ParentProcessID, e.szExeFile)
        ok = k32.Process32NextW(snap, ctypes.byref(e))
    k32.CloseHandle(snap)
    return table


def win_open(k32, pid, access):
    """(handle, start time as a FILETIME number) of a live process, or (None, None)."""
    import ctypes
    from ctypes import wintypes
    k32.OpenProcess.restype = wintypes.HANDLE
    k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    k32.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.c_void_p] * 4
    h = k32.OpenProcess(access, False, pid)
    if not h:
        return None, None
    t = [wintypes.FILETIME() for _ in range(4)]
    if not k32.GetProcessTimes(h, *(ctypes.byref(x) for x in t)):
        k32.CloseHandle(h)
        return None, None
    return h, (t[0].dwHighDateTime << 32) | t[0].dwLowDateTime


def win_note_tree(k32, root, since, seen):
    """Adds to seen {pid: (start, name)} every process below root or below a pid already seen that started after
    since (a FILETIME number: the wrapper's own start), so orphans whose parents died are still found by their parent
    ids."""
    # *Fixed 2026-11-17:* a round 11 build ended 9 of snatcherLeo's msedge.exe processes. The walk used to expand from
    # every pid ever noted, alive or not; once a noted process had exited and Windows reused its pid for an unrelated
    # program, that program's children (started after `since`) counted as ours and were terminated. Now a parent is
    # expanded only while it is still the same process we noted (same pid AND same start time), and a child must have
    # started no earlier than that parent. Orphans are still caught: the tree is noted every ~2 s while the parent
    # lives. Windows only; the POSIX path uses a process group instead (no such walk needed on Linux / macOS).
    table = win_process_table(k32)
    kids = {}
    for pid, (ppid, _) in table.items():
        kids.setdefault(ppid, []).append(pid)

    def live_start(pid):
        h, start = win_open(k32, pid, 0x1000)  # PROCESS_QUERY_LIMITED_INFORMATION
        if h:
            k32.CloseHandle(h)
        return start

    root_start = live_start(root)
    stack = [(root, root_start)] if root_start is not None else []
    for pid, (start, _) in seen.items():
        if live_start(pid) == start:  # still the process we noted, not a reused pid
            stack.append((pid, start))
    while stack:
        parent, parent_start = stack.pop()
        for c in kids.get(parent, []):
            if c in seen or c == os.getpid():
                continue
            start = live_start(c)
            if start is not None and start >= since and start >= parent_start:
                seen[c] = (start, table[c][1])
                stack.append((c, start))


def win_end_leftovers(k32, seen):
    """Ends every noted process that is still alive (same pid and start time); returns their names."""
    from ctypes import wintypes
    k32.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    ended = []
    for pid, (start, name) in seen.items():
        h, now = win_open(k32, pid, 0x1001)  # PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION
        if not h:
            continue
        if now == start and k32.TerminateProcess(h, 124):
            ended.append(f"{name} ({pid})")
        k32.CloseHandle(h)
    return ended


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--time", type=float, default=300)
    ap.add_argument("--mem-gb", type=float, default=8)
    ap.add_argument("--no-lock", action="store_true", help="only for light commands")
    ap.add_argument("--log", help="write the command's output here instead of the console")
    ap.add_argument("--why", help="one plain sentence for the run board: what this run is for (whatsrunning.py)")
    ap.add_argument("--exclusive", action="store_true",
                    help="run alone (every slot): Dolphin comparison runs (automatic) and timing-sensitive measurements "
                         "(audio, RNG counts, load stalls); snatcherLeo, 2026-10-17")
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    a = ap.parse_args()
    cmd = a.cmd[1:] if a.cmd[:1] == ["--"] else a.cmd
    if not cmd:
        ap.error("no command")
    import shutil
    exe = shutil.which(cmd[0]) or (os.path.abspath(cmd[0]) if os.path.exists(cmd[0]) else None)
    if exe:  # CreateProcess does not resolve a relative path written with forward slashes
        cmd = [exe] + cmd[1:]
    cmd = cap_jobs(cmd)
    # Dolphin runs are comparison runs: always alone (snatcherLeo, 2026-10-17)
    exclusive = a.exclusive or "dolphin" in Path(cmd[0]).name.lower()
    why = (a.why or "") + (" [exclusive]" if exclusive else "")
    env = dict(os.environ)
    env.setdefault("ORRE_WATCHDOG", "15")
    env.setdefault("ORRE_FRAME_LOG", "1")
    # B's automated-run window: the title names the lane from --why ("D: ..." -> D), "do not play" (snatcherLeo, 2026-10-17)
    env.setdefault("ORRE_RUN_WHY", a.why or "")
    if not a.no_lock:
        board_write("waiting", cmd, why, a.time)
        wait_while_player_plays()
    try:
        lock = None if a.no_lock else acquire_lock(exclusive, is_build(a.cmd))
    except BaseException:
        board_remove()
        raise
    if not a.no_lock:
        board_write("running", cmd, why, a.time)
    out = open(a.log, "wb") if a.log else None
    try:
        if os.name == "nt":
            try:
                k32, job, peak = windows_job(int(a.mem_gb * (1 << 30)))
            except OSError as e:
                print(f"[saferun] could not set up the job object: {e}", file=sys.stderr)
                return 125
            h, since = win_open(k32, os.getpid(), 0x1000)  # only processes started after this wrapper count
            if h:
                k32.CloseHandle(h)
            since = since or 0
            # start suspended, put it in the job, then let it run: nothing it starts escapes the limits
            p = subprocess.Popen(cmd, env=env, stdout=out, stderr=subprocess.STDOUT if out else None,
                                 creationflags=0x4 | 0x200)  # CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP
            import ctypes
            if not k32.AssignProcessToJobObject(job, int(p._handle)):
                p.kill()
                print("[saferun] could not assign the process to the job", file=sys.stderr)
                return 125
            ctypes.WinDLL("ntdll").NtResumeProcess(int(p._handle))
        else:
            p = subprocess.Popen(cmd, env=env, stdout=out, stderr=subprocess.STDOUT if out else None,
                                 start_new_session=True, preexec_fn=lambda: os.nice(5))
            peak = lambda: 0  # noqa: E731
        seen = {}  # Windows: the command's tree as it runs (win_note_tree), for processes that escape the job
        deadline = time.monotonic() + a.time
        rc = None
        while rc is None:
            try:
                rc = p.wait(timeout=max(0.05, min(2.0, deadline - time.monotonic())))
            except subprocess.TimeoutExpired:
                if os.name == "nt":
                    win_note_tree(k32, p.pid, since, seen)
                if time.monotonic() >= deadline:
                    break
        if rc is None:
            if os.name == "nt":
                win_note_tree(k32, p.pid, since, seen)
                k32.TerminateJobObject(job, 124)  # the whole tree inside the job
            else:
                os.killpg(p.pid, 9)
            p.wait()
            print(f"[saferun] time limit {a.time:.0f} s reached: killed", file=sys.stderr)
            rc = 124
        if os.name == "nt":
            ended = win_end_leftovers(k32, seen)
            if ended:
                print(f"[saferun] ended {len(ended)} leftover process(es) outside the job: {', '.join(ended)}",
                      file=sys.stderr)
            print(f"[saferun] exit {rc & 0xFFFFFFFF:#x}, peak memory {peak() / (1 << 30):.2f} GB "
                  f"(cap {a.mem_gb:g} GB)", file=sys.stderr)
            k32.CloseHandle(job)
        return rc
    finally:
        if out:
            out.close()
        if lock:
            lock.close()
        board_remove()


if __name__ == "__main__":
    sys.exit(main())
