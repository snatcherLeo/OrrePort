"""A the reference playthrough sitting's Dolphin side, closed loop (2026-11-03; P01's runner made general, moved here from a scratch
folder on 2026-11-03 so it is backed up).

    python -I tools/local/testing/run3_live.py <n> <code> <sitting folder in _run3> <pc replay run.log> [NAME=VALUE ...]

Makes the stand-in sitting _run3/dolphin/sittings/<n>_<code>_<stamp>/ if missing (P01's settings: AspectRatio 2,
SkipDuplicateXFBs False; the custom RTC set to the recording's start; an empty card unless the sitting has a start
card), builds the live-pad steps from the .orrepad and the PC replay's log (livepad_steps.py; the log needs
ORRE_MENU_LOG=1), then runs Dolphin with the rolls injected (PLAYREC_ROLLINJECT) through saferun, --exclusive.
NAME=VALUE pairs go into the environment first: RUN3_ROLLS (default: the sitting's .rolls), RUN3_PROBES (default
scripts,sfx,camera,pos,menu), RUN3_FRAMES (the stand-in's last frame, first run only), and any PLAYREC_* option.
Gives way: waits while an exclusive run is going, anyone else's run waits, or snatcherLeo plays. The replay ends itself on a
stall (the live pad's watchdog)."""
import glob
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time

FD = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DECOMP = os.path.dirname(FD)
MAIN = os.path.join(DECOMP, "pkmn_colosseum")
R3 = os.path.join(DECOMP, "_run3")
P01 = os.path.join(R3, "dolphin", "sittings", "101_P01_20261102_2049")

n, code, src, pclog = sys.argv[1:5]
for kv in sys.argv[5:]:                 # NAME=VALUE pairs go into this process's environment first (RUN3_ROLLS etc.)
    os.environ[kv.split("=", 1)[0]] = kv.split("=", 1)[1]
src = src if os.path.isabs(src) else os.path.join(R3, src)
rec = glob.glob(os.path.join(src, "*.orrepad"))[0]
rolls = os.environ.get("RUN3_ROLLS") or glob.glob(os.path.join(src, "*.rolls"))[0]   # (a PC replay's, with field lines)
stamp = re.search(r"(\d{8}_\d{4})", os.path.basename(rec)).group(1)
sit = os.path.join(R3, "dolphin", "sittings", "%s_%s_%s" % (n, code, stamp))
if not os.path.isdir(sit):
    os.makedirs(sit)
    shutil.copytree(os.path.join(P01, "settings"), os.path.join(sit, "settings"))
    # the recording's start as Dolphin's clock (the .orrepad header: OSTime ticks since 2000, 40.5 MHz)
    epoch = struct.unpack_from("<Q", open(rec, "rb").read(24), 16)[0]
    rtc = 946684800 + epoch // 40500000
    ini = os.path.join(sit, "settings", "Dolphin.ini")
    text = re.sub(r"CustomRTCValue = 0x[0-9A-Fa-f]+", "CustomRTCValue = 0x%X" % rtc, open(ini).read())
    open(ini, "w").write(text)
    json.dump({"last_frame": int(os.environ.get("RUN3_FRAMES", "130000")),
               "note": "the reference playthrough %s: a stand-in for playrec" % code}, open(os.path.join(sit, "sitting.json"), "w"))
    open(os.path.join(sit, "events.jsonl"), "w").close()
    starts = glob.glob(os.path.join(src, "start*.gci")) + glob.glob(os.path.join(src, "card_start", "*.gci"))
    if starts:
        os.makedirs(os.path.join(sit, "card_start"))
        shutil.copy2(starts[0], os.path.join(sit, "card_start", "01-GC6E-pokemon_colosseum.gci"))
steps = os.path.join(sit, "steps.json")
subprocess.check_call([sys.executable, "-I", os.path.join(FD, "tools", "local", "testing", "livepad_steps.py"), rec,
                       pclog, steps])


def busy():
    out = subprocess.run([sys.executable, "-I", os.path.join(MAIN, "tools", "local", "whatsrunning.py")],
                         capture_output=True, text=True, cwd=MAIN).stdout
    run = out.split("RUNNING NOW", 1)[1].split("WAITING", 1)[0] if "RUNNING NOW" in out else ""
    waiting = out.split("WAITING", 1)[1] if "WAITING" in out else ""
    return "[exclusive]" in run or "why:" in waiting or "snatcherLeo is playing: yes" in out


while busy():
    time.sleep(20)
env = dict(os.environ, PLAYREC_ISO=os.path.join(MAIN, "orig", "Pokemon Colosseum (USA).iso"),
           PLAYREC_ROLLINJECT=rolls)
for kv in sys.argv[5:]:
    k, v = kv.split("=", 1)
    env[k] = v
cmd = [sys.executable, "-I", os.path.join(MAIN, "tools", "local", "saferun.py"), "--exclusive", "--time", "3000",
       "--why", "D: the reference playthrough %s in Dolphin, the PC's pad fed live (closed loop), its rolls injected" % code, "--",
       sys.executable, "-I", "tools/local/testing/playrec.py", "--root", os.path.join(R3, "dolphin"),
       "--user", os.path.join(DECOMP, "_playthrough", "dolphin-user"),
       "replay", n, "--livepad", steps, "--probe", env.get("RUN3_PROBES", "scripts,sfx,camera,pos,menu")]
sys.exit(subprocess.call(cmd, cwd=FD, env=env))
