"""movesweep.py: the game's own pictures of a move, for checking Studio's move preview against it (release step 4).

Why: Studio plays a move's effects (particles, the target's half, the battle camera) by porting the game's code; the
only way to know each move is right is to put the game's frames beside Studio's. OrrePort's move sweep
(ORRE_WAZA_SWEEP, docs/port/setup.md "The move sweep") runs the game's own move viewer, Genius Sonority's debug tool,
playing a move on Espeon attacking Umbreon with the game's own particle code and battle camera. This tool:
  1. runs the sweep for one move on the replay clock (ORRE_VIRTUAL_CLOCK, so every run steps on the same frames) to
     learn the move's frame range from the log ("[waza_sweep] item ... game frames F-G");
  2. runs it again with ORRE_DUMP_FRAMES over that range (at most 32 pictures a run, so several runs) and keeps the
     pictures as <out>/<move>/game_<frame>.png, with each run's log (ORRE_BCAM_LOG=1 adds the battle camera's lines
     once OrrePort has the switch; harmless before).
Runs beside the project's sweeps without the shared lock (2026-11-19: "please run it with saferun --no-lock"), each
in a fresh folder (no deleting), from a copy of the project's exe (a relink deletes firstlink's exe first). Every run has a
stall check: no log growth for 60 s, or past its time cap, ends it.
Platforms: Windows (the game build, PowerShell for the process list); Linux / macOS would need their own exe and ps.

Usage: python -I studio/tests/movesweep.py --move 52 [--every 2] [--out <dir>]
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import allmodels as am  # noqa: E402  (the shared paths, the exe copy and the process helper)


def run_game(exe_dir, run_dir, env_extra, why, cap=600):
    os.makedirs(run_dir)
    env = dict(os.environ)
    env.update({'ORRE_WINDOW': 'minimized', 'ORRE_SKIP_HUB': '1', 'ORRE_VIRTUAL_CLOCK': '1', 'ORRE_REPLAY_SPEED': 'max',
                'ORRE_BCAM_LOG': '1', 'PATH': exe_dir + os.pathsep + am.MINGW + os.pathsep + env.get('PATH', '')})
    env.update(env_extra)
    log = os.path.join(run_dir, 'run.log')
    cmd = [sys.executable, '-I', am.SAFERUN, '--no-lock', '--time', str(cap), '--log', log, '--why', why, '--',
           os.path.join(exe_dir, 'orrecolo.exe'), '--disc', am.DISC]
    p = subprocess.Popen(cmd, cwd=run_dir, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    last_size, last_change, t0 = -1, time.time(), time.time()
    while p.poll() is None:
        time.sleep(1)
        size = os.path.getsize(log) if os.path.exists(log) else 0
        text = open(log, encoding='utf-8', errors='replace').read() if size else ''
        if '[waza_sweep] done' in text:
            time.sleep(2)  # (the last dumps finish writing)
            break
        if size != last_size:
            last_size, last_change = size, time.time()
        elif time.time() - last_change > 60:
            print('  stalled: no log output for 60 s, ended', flush=True)
            break
        if time.time() - t0 > cap:
            break
    for c in am.children_of(p.pid):
        subprocess.run(['taskkill', '/PID', str(c), '/T', '/F'], capture_output=True)
    try:
        p.wait(30)
    except subprocess.TimeoutExpired:
        p.kill()
    return open(log, encoding='utf-8', errors='replace').read() if os.path.exists(log) else ''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--move', type=int, required=True)
    ap.add_argument('--every', type=int, default=2, help='dump every n-th game frame of the move')
    ap.add_argument('--out', default=os.path.join(os.environ.get('TEMP', '.'), 'movesweep'))
    a = ap.parse_args()
    out = os.path.join(a.out, str(a.move))
    os.makedirs(out, exist_ok=True)
    exe_dir = am.copy_game(a.out)
    stamp = time.strftime('%H%M%S')
    sweep = {'ORRE_WAZA_SWEEP': 'main:%d-%d' % (a.move, a.move)}
    text = run_game(exe_dir, os.path.join(out, 'range_' + stamp), sweep,
                    'Studio move check: the game playing move %d once, to find its frames' % a.move)
    items = re.findall(r'\[waza_sweep\] item \d+ layout (\d+) move (\d+) (.*?): game frames (\d+)-(\d+)', text)
    if not items:
        sys.exit('no "[waza_sweep] item" line in the log (see %s)' % os.path.join(out, 'range_' + stamp, 'run.log'))
    first, last = min(int(i[3]) for i in items), max(int(i[4]) for i in items)
    print('move %d: %s; game frames %d-%d' % (a.move, '; '.join('layout %s %s %s-%s' % (i[0], i[2], i[3], i[4]) for i in items),
                                             first, last), flush=True)
    frames = list(range(first, last + 1, max(1, a.every)))
    for k in range(0, len(frames), 32):
        part = frames[k:k + 32]
        run_dir = os.path.join(out, 'dump_%s_%d' % (stamp, k // 32))
        run_game(exe_dir, run_dir, dict(sweep, ORRE_DUMP_FRAMES=','.join(map(str, part))),
                 'Studio move check: the game\'s pictures of move %d, frames %d-%d' % (a.move, part[0], part[-1]))
        got = 0
        fd = os.path.join(run_dir, 'frames')
        for f in sorted(os.listdir(fd)) if os.path.isdir(fd) else []:
            m = re.match(r'frame_(\d+)\.png$', f)
            if m:
                shutil.copy2(os.path.join(fd, f), os.path.join(out, 'game_%s.png' % m.group(1)))
                got += 1
        print('  frames %d-%d: %d pictures' % (part[0], part[-1], got), flush=True)
    open(os.path.join(out, 'range.txt'), 'w').write('%d %d\n' % (first, last) + ''.join('%s\n' % ' '.join(i) for i in items))


if __name__ == '__main__':
    main()
