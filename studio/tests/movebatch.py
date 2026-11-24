"""movebatch.py: side-by-side sheets of the game's moves against Studio's, many moves at once (release step 4).

Why: snatcherLeo (2026-11-19): "fix and ensure particles look exactly as they do in-game from the debug tools ... this needs
to be a faithful and very reliable tool". The first comparisons (Ember, Ice Beam) found real bugs (the mirrored
particle pictures), so every move gets the same check. For each move:
  1. the game's own pictures from OrrePort's move sweep (the game's move viewer, Espeon on Umbreon; movesweep.py's
     runner: the replay clock, a fresh folder, beside the project's runs without the shared lock as the project asked), N frames
     spread over the move;
  2. Studio's pictures of the same moments: Espeon with --any-move, Umbreon as the target (--target 197), each
     game frame g shown as Studio's move frame (g - first) / 2 (the battle runs 2 retraces a frame);
  3. a sheet <out>/sheets/<move>.png: one row per moment, the game's picture left, Studio's right (both 320 x 240).
Until the battle camera's log (ORRE_BCAM_LOG) feeds Studio the game's exact camera, the two cameras differ (each shot
is random); the sheets still show the effects' shapes, colours, sizes and timing.
Platforms: Windows (the game build and PowerShell, through movesweep / allmodels); the sheets are plain Python.

Usage: python -I studio/tests/movebatch.py --moves 1-20 [--frames 12] [--out <dir>]
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import allmodels as am  # noqa: E402
import movesweep as ms  # noqa: E402

ESPEON, UMBREON = 196, 197


def parse_moves(text):
    out = []
    for part in text.split(','):
        if '-' in part:
            a, b = part.split('-')
            out += list(range(int(a), int(b) + 1))
        elif part:
            out.append(int(part))
    return out


def game_cameras(d):
    """the game's battle camera at each retrace of the range run ({retrace: 'ex,ey,ez,ix,iy,iz,fov'}), from its
    ORRE_BCAM_LOG lines, so Studio can draw each moment from the same camera (--shot-camera)"""
    cams = {}
    for f in os.listdir(d):
        log = os.path.join(d, f, 'run.log')
        if not f.startswith('range_') or not os.path.exists(log):
            continue
        for line in open(log, errors='replace'):
            if not line.startswith('[bcam] frame ') and not line.startswith('[bcam] picked '):
                continue
            kv = dict(p.split('=', 1) for p in line.split()[2:] if '=' in p)
            if 'eye' in kv and 'interest' in kv and 'fov' in kv:
                at = int(kv['frame']) if 'frame' in kv else None
                if at is not None:
                    cams[at] = '%s,%s,%s' % (kv['eye'], kv['interest'], kv['fov'])
        break
    return cams


def game_pictures(move, frames_wanted, out, exe_dir, span=None):
    """the game's pictures of the move: {retrace frame: path}, and the move's first frame. span (a, b, step): those
    retraces after the move's first frame instead of frames_wanted spread over the whole move (a closer look)"""
    d = os.path.join(out, 'game', str(move) + ('' if not span else '_%d_%d_%d' % span))
    os.makedirs(d, exist_ok=True)
    have = {int(m.group(1)): os.path.join(d, f) for f in os.listdir(d) for m in [re.match(r'g_(\d+)\.png$', f)] if m}
    rng = os.path.join(d, 'range.txt')
    if have and os.path.exists(rng):
        return have, int(open(rng).read().split()[0]), game_cameras(d)
    stamp = time.strftime('%H%M%S')
    sweep = {'ORRE_WAZA_SWEEP': 'main:%d-%d' % (move, move)}
    text = ms.run_game(exe_dir, os.path.join(d, 'range_' + stamp), sweep, 'Studio move check: the frames of move %d' % move)
    items = re.findall(r'\[waza_sweep\] item \d+ layout (\d+) move (\d+) (.*?): game frames (\d+)-(\d+)', text)
    if not items:
        return {}, 0, {}
    first, last = min(int(i[3]) for i in items), max(int(i[4]) for i in items)
    if span:
        want = list(range(first + span[0], min(last, first + span[1]) + 1, span[2]))[:32]
    else:
        step = max(2, (last - first) // max(1, frames_wanted - 1))
        step += step % 2  # (even: whole battle frames)
        want = list(range(first, last + 1, step))[:32]
    run_dir = os.path.join(d, 'dump_' + stamp)
    ms.run_game(exe_dir, run_dir, dict(sweep, ORRE_DUMP_FRAMES=','.join(map(str, want))),
                'Studio move check: the game\'s pictures of move %d' % move)
    fd = os.path.join(run_dir, 'frames')
    for f in sorted(os.listdir(fd)) if os.path.isdir(fd) else []:
        m = re.match(r'frame_(\d+)\.png$', f)
        if m:
            dst = os.path.join(d, 'g_%s.png' % m.group(1))
            shutil.copy2(os.path.join(fd, f), dst)
            have[int(m.group(1))] = dst
    open(rng, 'w').write('%d %d %s\n' % (first, last, ' '.join(i[2] for i in items)))
    return have, first, game_cameras(d)


STAGE = '--plain'  # (--arena on the command line: Studio's Arena stage, the game's own room)
# Substitute, Thief, Helping Hand, Wish, Snatch, Covet: the event class in waza_func_data (studio/win32/app.cpp
# event_part_move, verified from the code)
EVENT_MOVES = {164, 168, 270, 273, 289, 343}


def studio_picture(move, frame, path, camera=None):
    env = dict(os.environ)
    env['PATH'] = am.MINGW + os.pathsep + env.get('PATH', '')
    # the game's pictures come from its own move viewer room, the Phenac Colosseum; Studio's Arena became Mt. Battle's
    # valley on 2026-11-22 (snatcherLeo's pick), so the sweep asks for Phenac's room to compare like with like
    env.setdefault('ORRE_ARENA', 'waza_viewer:3')
    cmd = [sys.executable, '-I', am.SAFERUN, '--no-lock', '--time', '60', '--why', 'Studio move check: move %d frame %d' % (move, frame),
           '--', os.path.join(am.STUDIO, 'orrestudio.exe'), '--disc', am.DISC, '--select', str(ESPEON), '--tab', '1',
           '--learn-group', '0', '--any-move', str(move), '--learn-move', str(move), '--target', str(UMBREON), STAGE,
           '--play', '--hold-frame', str(frame), '--shot-model', '--size', '800x700', '--screenshot', path]
    # (800 x 700: at 800 x 600 the Moves page runs past the window's bottom and the 3D view's picture is cut, so it is
    # no longer centred on the camera's aim; Studio's log warns "[screenshot] the 3D view ... is cut")
    if camera:
        cmd[cmd.index('--play'):cmd.index('--play')] = ['--shot-camera', camera]
    # the event moves (the parts picker: Move / Follow-up / Both) play both parts in a battle, as the game's capture
    # shows, so Studio plays Both here (its picker starts on Move). *Added 2026-11-21:* Substitute 164 and Thief 168
    # looked wrong in the 151-180 sweep only because Studio played the Move part alone
    if move in EVENT_MOVES:
        cmd[cmd.index('--play'):cmd.index('--play')] = ['--parts', '2']
    cmd.insert(cmd.index('--select'), '--log-detail')
    # (plain visible windows: snatcherLeo, 2026-11-19, "VIsible windows are fine ... I don't mind"; the nearly transparent
    # --quiet-window left faint ghost traces on his monitor)
    subprocess.run(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    # the move Studio really played (its log's "[move] move N part P" line): a move missing from the chosen list once
    # played another one silently (Shadow Ball showed Tackle, 2026-11-19)
    logs = os.path.join(am.STUDIO, 'logs')
    newest = max((os.path.join(logs, f) for f in os.listdir(logs)), key=os.path.getmtime, default=None) if os.path.isdir(logs) else None
    played = re.findall(r'\[move\] move (\d+) part', open(newest, errors='replace').read()) if newest else []
    if str(move) not in played:
        print('  WARNING: move %d frame %d: Studio played %s' % (move, frame, ', '.join(played) or 'no move'), flush=True)
    return os.path.exists(path)


def crop43(img):
    """the centre 4:3 of a wider picture: Studio's preview is wider than the game's 640 x 480, and its projection keeps
    the vertical field of view, so the centre 4:3 is the game's own framing (from the same camera)"""
    h, w = len(img), len(img[0]) if img else 0
    cw = min(w, h * 4 // 3)
    x0 = (w - cw) // 2
    return [row[x0:x0 + cw] for row in img]


def sheet(rows, path):
    W, H = 320, 240
    img = []
    for game, studio in rows:
        g = am.resize(am.png_load(game), W, H) if game and os.path.exists(game) else [[(0, 0, 0)] * W for _ in range(H)]
        s = am.resize(crop43(am.png_load(studio)), W, H) if studio and os.path.exists(studio) else [[(0, 0, 0)] * W for _ in range(H)]
        for y in range(H):
            img.append(g[y] + [(255, 255, 255)] * 4 + s[y])
        img += [[(255, 255, 255)] * (2 * W + 4)] * 4
    am.png_write(path, img)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--moves', required=True, help='e.g. 1-20,52,58')
    ap.add_argument('--frames', type=int, default=12)
    ap.add_argument('--out', default=os.path.join(os.environ.get('TEMP', '.'), 'movebatch'))
    ap.add_argument('--span', help='a,b,step: game retraces a..b after the move starts, every step (a closer look)')
    ap.add_argument('--own-camera', action='store_true', help="Studio's own battle camera instead of the game's")
    ap.add_argument('--studio-tag', default='', help="a suffix for Studio's picture folder (after a new Studio build)")
    ap.add_argument('--arena', action='store_true', help="Studio's Arena stage (the game's own room) instead of Plain stage")
    a = ap.parse_args()
    global STAGE
    if a.arena:
        STAGE = '--arena'
        a.studio_tag += '_arena'
    span = tuple(int(x) for x in a.span.split(',')) if a.span else None
    os.makedirs(os.path.join(a.out, 'sheets'), exist_ok=True)
    exe_dir = am.copy_game(a.out)
    for move in parse_moves(a.moves):
        t0 = time.time()
        pics, first, cams = game_pictures(move, a.frames, a.out, exe_dir, span)
        if not pics:
            print('move %d: no game pictures' % move, flush=True)
            continue
        tag = '' if not span else '_%d_%d_%d' % span
        # (a new Studio build: --studio-tag names a fresh folder for its pictures, keeping the game's, which don't change)
        sd = os.path.join(a.out, 'studio' + ('' if a.own_camera else '_gcam') + a.studio_tag, str(move))
        os.makedirs(sd, exist_ok=True)
        rows = []
        for g in sorted(pics):
            f = (g - first) // 2
            # the game's camera the picture was drawn with (the log's nearest line at or before g - 2), unless Studio's
            # own was asked for. The picture dumped at retrace g shows the camera of one battle frame earlier: Gust's
            # cut is logged at 422 (the new shot's first frame), yet the picture at 423 still shows the old shot
            # (2026-11-20); within a shot the difference is tiny, at a cut it paired the picture with the wrong shot
            cam = None
            if not a.own_camera and cams:
                at = max((r for r in cams if r <= g - 2), default=None)
                if at is None:
                    at = max((r for r in cams if r <= g), default=None)
                cam = cams[at] if at is not None else None
            sp = os.path.join(sd, 's_%04d_%d.png' % (f, g))
            if not os.path.exists(sp):
                studio_picture(move, f, sp, cam)
            rows.append((pics[g], sp))
        sheet(rows, os.path.join(a.out, 'sheets', '%03d%s%s.png' % (move, tag, '' if a.own_camera else '_gcam')))
        print('move %d: %d moments, %.0f s' % (move, len(rows), time.time() - t0), flush=True)


if __name__ == '__main__':
    main()
