r"""Crash recovery for a sitting (the reference playthrough Part 1 item 3, 2026-11-01; docs/port/pad-recording.md).

    python -I tools/local/port/recover_sitting.py <portable folder> <recording.orrepad> [--until-frame F]
           [--seconds-before S] [--restore-save]

Prepares a replay of the sitting up to just before the crash, which then hands the controller back:
  - recover.pad: the recording as a pad script up to game frame F (default: S seconds, 5, before the recording's last
    record, i.e. before the crash);
  - recover.ps1: the PowerShell 5.1 launcher: the recorded clock start (ORRE_VIRTUAL_EPOCH), the replay at full speed
    (ORRE_REPLAY_SPEED=max) in a visible window (ORRE_WINDOW=normal), the hand-over after the last step (the pace
    switches to real time there, pc_vi_pace_real_time), and a new recording of the whole sitting (ORRE_PAD_RECORD);
  - the sitting's starting save: the launch backup made when the recording started
    (Colosseum\Saves\Backups\<time>\Slot 1\). The replay is exact only from that save. --restore-save copies it into
    Colosseum\Saves\Slot 1\ after copying the slot's current files to Backups\<now>_before_recovery\Slot 1\ (nothing is
    deleted); without it the script only says which backup to restore (overwriting the save is the player's call).
Everything goes to Colosseum\Recordings\recovery_<time>\. Limits: it assumes the sitting ran with the same preset as
the replay (the reference playthrough: GameCube-accurate); the exe should be the build that recorded it, or one whose fix does not change
anything before the crash point (the replay's rooms show it).
"""
import argparse
import datetime
import glob
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import orrepad_to_pad as o2p  # noqa: E402


def stamp_time(name):
    """sitting_YYYYMMDD_HHMMSS.orrepad / YYYYMMDD_HHMMSS -> datetime, or None"""
    digits = "".join(ch for ch in os.path.basename(name) if ch.isdigit())[-14:]
    try:
        return datetime.datetime.strptime(digits, "%Y%m%d%H%M%S")
    except ValueError:
        return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder", help="the portable OrrePort folder (with orrecolo.exe and Colosseum\\)")
    ap.add_argument("recording", help="the sitting's .orrepad")
    ap.add_argument("--until-frame", type=int)
    ap.add_argument("--seconds-before", type=float, default=5.0)
    ap.add_argument("--restore-save", action="store_true", help="put the sitting's starting save back into Slot 1")
    a = ap.parse_args()

    folder = os.path.abspath(a.folder)
    stamp, records, epoch = o2p.read_recording(a.recording, with_epoch=True)
    if not records:
        sys.exit("the recording has no records")
    last_frame = records[-1][2]
    until = a.until_frame if a.until_frame is not None else max(0, last_frame - int(a.seconds_before * 60))
    now = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    out = os.path.join(folder, "Colosseum", "Recordings", f"recovery_{now}")
    os.makedirs(out)
    pad = os.path.join(out, "recover.pad")
    rc = subprocess.run([sys.executable, "-I", os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                             "orrepad_to_pad.py"), a.recording, pad, "--until-frame",
                         str(until)]).returncode
    if rc != 0:
        sys.exit("orrepad_to_pad failed")

    # the launch backup made when this recording started (the card is set up just before the recorder starts)
    started = stamp_time(a.recording)
    backups = sorted(glob.glob(os.path.join(folder, "Colosseum", "Saves", "Backups", "*", "Slot 1")))
    best = None
    for b in backups:
        t = stamp_time(os.path.dirname(b))
        if t is not None and started is not None and abs((started - t).total_seconds()) <= 10:
            best = b
    slot = os.path.join(folder, "Colosseum", "Saves", "Slot 1")
    if best is None:
        print("no launch backup within 10 s of the recording's start: the replay needs the save the sitting began with")
    elif a.restore_save:
        keep = os.path.join(folder, "Colosseum", "Saves", "Backups", f"{now}_before_recovery", "Slot 1")
        os.makedirs(keep)
        for g in glob.glob(os.path.join(slot, "*.gci")):
            shutil.copy2(g, keep)
        for g in glob.glob(os.path.join(best, "*.gci")):
            shutil.copy2(g, slot)
        print(f"save restored from {best} (the slot's files before it: {keep})")
    else:
        print(f"the sitting's starting save: {best}\n  restore it into {slot} before launching (or rerun with "
              f"--restore-save, which keeps a copy of the current files)")

    record = os.path.join(out, "sitting_recovered.orrepad")
    exe = os.path.join(folder, "orrecolo.exe")
    lines = [f"# OrrePort crash recovery, made {now} from {a.recording} (build '{stamp}')",
             f"# replays to game frame {until} (the recording ends at {last_frame}), then hands the controller back",
             f'$env:ORRE_PAD_SCRIPT = "@file:{pad}"',
             '$env:ORRE_WINDOW = "normal"',
             '$env:ORRE_REPLAY_SPEED = "max"',
             f'$env:ORRE_PAD_RECORD = "{record}"']
    if epoch:
        lines.append(f'$env:ORRE_VIRTUAL_EPOCH = "{epoch}"')
    lines += [f'& "{exe}"',
              "Remove-Item Env:ORRE_PAD_SCRIPT, Env:ORRE_WINDOW, Env:ORRE_REPLAY_SPEED, Env:ORRE_PAD_RECORD "
              "-ErrorAction SilentlyContinue"]
    if epoch:
        lines.append("Remove-Item Env:ORRE_VIRTUAL_EPOCH -ErrorAction SilentlyContinue")
    with open(os.path.join(out, "recover.ps1"), "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"recovery prepared in {out}: run recover.ps1 (PowerShell) to replay to frame {until} and take over")


if __name__ == "__main__":
    main()
