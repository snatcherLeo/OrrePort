# allmodels.py: the all-models check (snatcherLeo, 2026-11-19: "check and verify all of these models are perfected").
#
# Why: Studio draws every battle model with its own renderer. The only proof that it presents a model as the game does
# is the game's own picture of the same model, on the same camera, on the same held frame. OrrePort can boot straight
# into the developers' move viewer (ORRE_WAZA_VIEWER, the project's switch, docs/port/setup.md "Booting into the move
# viewer"); this script takes that shot for every species, renders Studio's version with the camera the game logged
# (--game-camera, --plain for the same grey backdrop, --hold-frame 0), and compares the two:
#   - the outline (strict): which pixels are model and which are backdrop must agree, apart from a 2-pixel edge band
#     (anti-aliasing and rasterisation differ). A wrong pose, a missing or extra part, broken skinning all show here;
#   - the colour (loose): the mean difference where both have the model. Studio has no lighting yet, so this only
#     catches gross mistakes (a wrong picture, a missing texture stage) until the lighting toggle lands.
# The report ranks species by the outline error, with side-by-side pictures (game | Studio | where they differ).
#
# Steps (each can be re-run alone; finished items are skipped unless --redo):
#   python -I studio/tests/allmodels.py framing --out DIR       Studio's model bounds -> DIR/framing.tsv
#   python -I studio/tests/allmodels.py game    --out DIR [...]  the game's shots -> DIR/game/<sp>.png + .camera.txt
#   python -I studio/tests/allmodels.py studio  --out DIR [...]  Studio's renders -> DIR/studio/<sp>.png
#   python -I studio/tests/allmodels.py compare --out DIR        DIR/report.tsv, DIR/sheets/*.png
# Options: --species 4,319,348 (a list; default: every species in framing.tsv), --lock (game shots wait in saferun's
# shared queue instead of --no-lock), --redo.
#
# Platform: the game half runs Windows' orrecolo.exe and ends it with taskkill (Windows only; Linux / macOS would end
# the child with a signal, likely the same otherwise). Plain Python, no packages (PNG read and written by hand).
import argparse
import math
import os
import shutil
import struct
import subprocess
import sys
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
WORKTREE = os.path.normpath(os.path.join(HERE, '..', '..'))
# the main checkout (its build, the disc, saferun): the folder holding git's shared folder, so a linked worktree
# anywhere finds it, and the main checkout finds itself; without git, this checkout is taken as the main one (every OS)
def _main_checkout():
    try:
        import subprocess
        common = subprocess.run(['git', '-C', WORKTREE, 'rev-parse', '--path-format=absolute', '--git-common-dir'],
                                capture_output=True, text=True, timeout=10).stdout.strip()
        if common:
            return os.path.normpath(os.path.dirname(common))
    except Exception:
        pass
    return WORKTREE


MAIN = _main_checkout()
DISC = os.path.join(MAIN, 'orig', 'Pokemon Colosseum (USA).iso')
SAFERUN = os.path.join(MAIN, 'tools', 'local', 'saferun.py')
GAME_DIR = os.path.join(MAIN, 'build-pc', 'firstlink')
STUDIO = os.path.join(WORKTREE, 'build-pc', 'studio')
MINGW = r'C:\msys64\mingw64\bin'
DUMP_FRAME = 300   # the model appears about frame 190 and held frames are identical from 260 on (setup.md)
GREY = 127         # the move viewer's backdrop, 0x7F7F7F
SUF = ''           # '' or '_lit' (--game-lighting): which Studio renders, report and sheets a step uses
SHINY = False      # --shiny: the shiny models (the game's ",shiny" viewer switch, Studio's --shiny); use its own --out
BAR_ROWS = 70      # the "GSvtr" banner across the top of every game shot (the game's own debug tool)
# species with their own effects (particle banks or glow cards; studio/effects-inventory.md): their particles are random
# and, on a held frame, the game seems to restart frame-0 emitters (Charmander's flame grows past Studio's; likely), so
# the report marks them and their outline error includes the effect, to be judged by eye
EFFECTS = {4, 5, 6, 92, 109, 110, 126, 134, 144, 146, 219, 321, 170, 171, 196, 313}


# ---- PNG (8-bit RGB / RGBA, any filter) ----
def png_load(p):
    d = open(p, 'rb').read()
    pos, idat, w, h, ct = 8, b'', 0, 0, 2
    while pos < len(d):
        n, t = struct.unpack('>I4s', d[pos:pos + 8])
        c = d[pos + 8:pos + 8 + n]
        if t == b'IHDR': w, h, _, ct = struct.unpack('>IIBB', c[:10])
        if t == b'IDAT': idat += c
        pos += 12 + n
    bpp = 4 if ct == 6 else 3
    raw = zlib.decompress(idat)
    st, prev, i, img = w * bpp, bytearray(w * bpp), 0, []
    for _ in range(h):
        f = raw[i]; line = bytearray(raw[i + 1:i + 1 + st]); i += 1 + st
        if f == 1:
            for x in range(bpp, st): line[x] = (line[x] + line[x - bpp]) & 255
        elif f == 2:
            for x in range(st): line[x] = (line[x] + prev[x]) & 255
        elif f == 3:
            for x in range(st): line[x] = (line[x] + ((line[x - bpp] if x >= bpp else 0) + prev[x]) // 2) & 255
        elif f == 4:
            for x in range(st):
                a = line[x - bpp] if x >= bpp else 0; b = prev[x]; c = prev[x - bpp] if x >= bpp else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        img.append([tuple(line[x * bpp:x * bpp + 3]) for x in range(w)])
        prev = line
    return img


def png_write(path, img):
    h, w = len(img), len(img[0])
    raw = b''.join(b'\x00' + bytes(v for p in row for v in p) for row in img)
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def resize(img, W, H):
    # box filter when shrinking (Studio's capture is larger than the game's 640 x 480), so edges average as the
    # game's single-sample pixels would see them more closely than a point sample
    h, w = len(img), len(img[0])
    out = []
    for y in range(H):
        ya, yb = int(y * h / H), max(int(y * h / H) + 1, int((y + 1) * h / H))
        line = []
        for x in range(W):
            xa, xb = int(x * w / W), max(int(x * w / W) + 1, int((x + 1) * w / W))
            n, s = 0, [0, 0, 0]
            for yy in range(ya, min(yb, h)):
                row = img[yy]
                for xx in range(xa, min(xb, w)):
                    p = row[xx]; s[0] += p[0]; s[1] += p[1]; s[2] += p[2]; n += 1
            line.append((s[0] // n, s[1] // n, s[2] // n))
        out.append(line)
    return out


# ---- framing: where to aim, from Studio's model bounds ----
def read_framing(out):
    rows = {}
    for line in open(os.path.join(out, 'framing.tsv')):
        sp, cy, r = line.split('\t')
        rows[int(sp)] = (float(cy), float(r))
    return rows


def camera_for(cy, r):
    # the viewer's preset angle (eye 30 up and 70 back, so it looks down about 23 degrees), pulled back for big
    # Pokémon so the eye stays outside them, aimed at the model's middle, the field of view fitted so the model's
    # sphere fills about 70% of the picture's height (clear of the GSvtr banner at the top)
    dist = max(70.0, 3.0 * r)
    height = 30.0 * dist / 70.0
    eye_y = 14.0 + height                    # (_AIM_Y keeps the eye where _HEIGHT puts it; the preset aim is 14)
    d = math.hypot(dist, eye_y - cy)
    fov = 2 * math.degrees(math.atan(1.45 * r / d))
    return {'ORRE_WAZA_VIEWER_ANGLE': '0', 'ORRE_WAZA_VIEWER_DISTANCE': '%.3f' % dist,
            'ORRE_WAZA_VIEWER_HEIGHT': '%.3f' % height, 'ORRE_WAZA_VIEWER_AIM_Y': '%.3f' % cy,
            'ORRE_WAZA_VIEWER_FOV': '%.3f' % fov}


def species_list(args, out):
    if args.species: return [int(s, 0) for s in args.species.split(',')]
    return sorted(read_framing(out))


# ---- the game's shots ----
def copy_game(out):
    # run from our own copy of the exe (a relink deletes firstlink's exe first; snatcherLeo via 2026-10-19): the exe,
    # its DLLs and the folders it reads at start (ui, Fonts); user/ is left out, so every run starts on default settings
    # one copy per build of the exe (named by its modified time), so a relink by Main gives a fresh copy, not a stale one
    stamp = time.strftime('%Y%m%d_%H%M%S', time.localtime(os.stat(os.path.join(GAME_DIR, 'orrecolo.exe')).st_mtime))
    dst = os.path.join(out, 'exe_' + stamp)
    if os.path.exists(os.path.join(dst, 'orrecolo.exe')): return dst
    os.makedirs(dst, exist_ok=True)
    for f in os.listdir(GAME_DIR):
        if f.endswith('.dll') or f == 'orrecolo.exe': shutil.copy2(os.path.join(GAME_DIR, f), dst)
    for sub in ('ui', 'Fonts'):
        if os.path.isdir(os.path.join(GAME_DIR, sub)): shutil.copytree(os.path.join(GAME_DIR, sub), os.path.join(dst, sub))
    st = os.stat(os.path.join(dst, 'orrecolo.exe'))
    open(os.path.join(out, 'exe_stamp.txt'), 'w').write('copied %s from %s, exe %d bytes, modified %s\n' % (
        time.strftime('%Y-%m-%d %H:%M:%S'), GAME_DIR, st.st_size, time.ctime(st.st_mtime)))
    return dst


def children_of(pid):
    # Windows: the processes whose parent is pid (saferun's child is the game)
    r = subprocess.run(['powershell', '-NoProfile', '-Command',
                        "(Get-CimInstance Win32_Process -Filter 'ParentProcessId=%d').ProcessId" % pid],
                       capture_output=True, text=True)
    return [int(x) for x in r.stdout.split() if x.strip().isdigit()]


def game_shot(sp, cam, exe_dir, out, lock):
    run = os.path.join(out, 'game', 'run_%d_%s' % (sp, time.strftime('%H%M%S')))  # a fresh folder per run (no rm)
    os.makedirs(run)
    env = dict(os.environ)
    env.update(cam)
    env.update({'ORRE_WAZA_VIEWER': str(sp) + (',shiny' if SHINY else ''), 'ORRE_WAZA_VIEWER_BACKDROP': '1', 'ORRE_WAZA_VIEWER_FRAME': '0',
                'ORRE_WAZA_VIEWER_LIGHTS': '1', 'ORRE_DUMP_FRAMES': str(DUMP_FRAME), 'ORRE_WINDOW': 'minimized',
                'ORRE_SKIP_HUB': '1', 'PATH': exe_dir + os.pathsep + MINGW + os.pathsep + env.get('PATH', '')})
    # every part's textures and material as the game binds them (the project's _TEXLOG, 7488742f1), for the exact data check
    env['ORRE_WAZA_VIEWER_TEXLOG'] = '1'
    log = os.path.join(run, 'run.log')
    cmd = [sys.executable, '-I', SAFERUN] + ([] if lock else ['--no-lock']) + [
        '--time', '90', '--log', log, '--why', 'All-models check: the game move viewer shot of species %d for Studio' % sp,
        '--', os.path.join(exe_dir, 'orrecolo.exe'), '--disc', DISC]
    p = subprocess.Popen(cmd, cwd=run, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    # (the dump lands on the nearest frame the tool can reach, e.g. frame_000301.png for 300: take whichever came)
    def find_pic():
        d = os.path.join(run, 'frames')
        got = sorted(f for f in os.listdir(d) if f.startswith('frame_') and f.endswith('.png')) if os.path.isdir(d) else []
        return os.path.join(d, got[0]) if got else os.path.join(d, 'none.png')
    pic = find_pic()
    # the stall check: the shot comes about 10 s after start; no picture 75 s after start (or 60 s after saferun let it
    # start, for a queued run) ends the run as "no picture" (saferun's 90 s cap is the backstop)
    t0, started, last = time.time(), None, -1
    while p.poll() is None:
        time.sleep(0.5)
        if started is None and children_of(p.pid): started = time.time()
        pic = find_pic()
        if os.path.exists(pic):
            size = os.path.getsize(pic)
            if size == last and size > 0: break
            last = size
        if (started and time.time() - started > 60) or (not lock and time.time() - t0 > 75): break
    for c in children_of(p.pid):
        subprocess.run(['taskkill', '/PID', str(c), '/T', '/F'], capture_output=True)
    try: p.wait(30)
    except subprocess.TimeoutExpired: p.kill()
    pic = find_pic()
    ok = os.path.exists(pic) and os.path.getsize(pic) > 0
    lines = []
    if os.path.exists(log):
        lines = [l for l in open(log, encoding='utf-8', errors='replace') if '[waza_viewer]' in l]
    if ok:
        shutil.copy2(pic, os.path.join(out, 'game', '%d.png' % sp))
        open(os.path.join(out, 'game', '%d.camera.txt' % sp), 'w', encoding='utf-8').writelines(lines)
    return ok, time.time() - t0


def cmd_game(args, out):
    fr = read_framing(out)
    exe_dir = copy_game(out)
    os.makedirs(os.path.join(out, 'game'), exist_ok=True)
    fails = 0
    for sp in species_list(args, out):
        if not args.redo and os.path.exists(os.path.join(out, 'game', '%d.png' % sp)): continue
        ok, secs = game_shot(sp, camera_for(*fr[sp]), exe_dir, out, args.lock)
        print('game %d: %s (%.0f s)' % (sp, 'ok' if ok else 'NO PICTURE', secs), flush=True)
        fails = 0 if ok else fails + 1
        if fails >= 5: sys.exit('five shots in a row gave no picture: stopped (look at the last run folder\'s run.log)')


# ---- Studio's renders ----
def cmd_studio(args, out):
    os.makedirs(os.path.join(out, 'studio' + SUF), exist_ok=True)
    env = dict(os.environ)
    env['PATH'] = MINGW + os.pathsep + env.get('PATH', '')
    for sp in species_list(args, out):
        cam = os.path.join(out, 'game', '%d.camera.txt' % sp)
        dst = os.path.join(out, 'studio' + SUF, '%d.png' % sp)
        if not os.path.exists(cam) or (not args.redo and os.path.exists(dst)): continue
        cmd = [sys.executable, '-I', SAFERUN, '--no-lock', '--time', '60', '--why', 'All-models check: Studio render of species %d' % sp,
               '--', os.path.join(STUDIO, 'orrestudio.exe'), '--disc', DISC, '--orreport', GAME_DIR,
               '--fonts', os.path.join(GAME_DIR, 'Fonts'), '--screenshot', dst, '--shot-model', '--size', '1500x900',
               '--select', str(sp), '--hold-frame', '0', '--game-camera', cam, '--plain'] + (['--game-lighting'] if SUF else ['--studio-lighting']) + (
               ['--shiny'] if SHINY else [])
        subprocess.run(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print('studio %d: %s' % (sp, 'ok' if os.path.exists(dst) else 'NO PICTURE'), flush=True)


# ---- the comparison ----
def mask_of(img):
    # model = anything not the backdrop grey; the banner rows are left out of both pictures. Black bars (the game's
    # backdrop quad is sized from the model's bound and may not reach the edges) count as backdrop when a whole row
    # or column run of pure black touches the picture's edge
    h, w = len(img), len(img[0])
    m = [[0] * w for _ in range(h)]
    for y in range(BAR_ROWS, h):
        row, mr = img[y], m[y]
        for x in range(w):
            r, g, b = row[x]
            if abs(r - GREY) > 4 or abs(g - GREY) > 4 or abs(b - GREY) > 4: mr[x] = 1
    for y in range(BAR_ROWS, h):  # black bars: pure black reaching the left / right edge
        for rng in (range(w), range(w - 1, -1, -1)):
            for x in rng:
                if img[y][x] != (0, 0, 0): break
                m[y][x] = 0
    for x in range(w):  # and the top / bottom edge
        for rng in (range(BAR_ROWS, h), range(h - 1, BAR_ROWS - 1, -1)):
            for y in rng:
                if img[y][x] != (0, 0, 0): break
                m[y][x] = 0
    return m


def near(m, x, y, v, rad=2):
    h, w = len(m), len(m[0])
    for yy in range(max(0, y - rad), min(h, y + rad + 1)):
        row = m[yy]
        for xx in range(max(0, x - rad), min(w, x + rad + 1)):
            if row[xx] == v: return True
    return False


def compare_one(g, s):
    mg, ms = mask_of(g), mask_of(s)
    h, w = len(g), len(g[0])
    area_g = area_s = both = only_g = only_s = 0
    cdiff = 0
    diff = [[(GREY, GREY, GREY)] * w for _ in range(h)]
    for y in range(BAR_ROWS, h):
        for x in range(w):
            a, b = mg[y][x], ms[y][x]
            area_g += a; area_s += b
            if a and b:
                both += 1
                p, q = g[y][x], s[y][x]
                cdiff += abs(p[0] - q[0]) + abs(p[1] - q[1]) + abs(p[2] - q[2])
                diff[y][x] = (200, 200, 200)
            elif a and not near(ms, x, y, 1):
                only_g += 1; diff[y][x] = (230, 40, 40)    # red: the game has model here, Studio doesn't
            elif b and not near(mg, x, y, 1):
                only_s += 1; diff[y][x] = (40, 90, 230)    # blue: Studio has model here, the game doesn't
    union = max(1, area_g + area_s - both)
    return {'area_game': area_g, 'area_studio': area_s, 'iou': both / union,
            'outline_err': (only_g + only_s) / max(1, area_g), 'only_game': only_g, 'only_studio': only_s,
            'colour_diff': cdiff / max(1, both) / 3}, diff


def crop_box(imgs):
    # the region around the model in either picture, padded, so the sheets show the Pokémon large
    h, w = len(imgs[0]), len(imgs[0][0])
    x0, y0, x1, y1 = w, h, 0, 0
    for img in imgs:
        m = mask_of(img)
        for y in range(BAR_ROWS, h):
            for x in range(w):
                if m[y][x]: x0, x1, y0, y1 = min(x0, x), max(x1, x), min(y0, y), max(y1, y)
    if x1 <= x0: return 0, 0, w, h
    pad = 12
    return max(0, x0 - pad), max(0, y0 - pad), min(w, x1 + pad + 1), min(h, y1 + pad + 1)


def cmd_compare(args, out):
    os.makedirs(os.path.join(out, 'sheets' + SUF), exist_ok=True)
    names = {}
    if os.path.exists(os.path.join(out, 'names.tsv')):
        for line in open(os.path.join(out, 'names.tsv'), encoding='utf-8'):
            k, v = line.rstrip('\n').split('\t', 1); names[int(k)] = v
    rows = []
    for sp in species_list(args, out):
        gp, sp_ = os.path.join(out, 'game', '%d.png' % sp), os.path.join(out, 'studio' + SUF, '%d.png' % sp)
        if not (os.path.exists(gp) and os.path.exists(sp_)): continue
        g = png_load(gp)
        s = resize(png_load(sp_), len(g[0]), len(g))
        r, diff = compare_one(g, s)
        r['species'] = sp
        rows.append(r)
        x0, y0, x1, y1 = crop_box([g, s])
        tiles = [[row[x0:x1] for row in img[y0:y1]] for img in (g, s, diff)]
        z = max(1, min(4, 300 // max(1, y1 - y0)))
        big = [[[px for px in row for _ in range(z)] for row in t for _ in range(z)] for t in tiles]
        gap = [(255, 255, 255)] * 8
        png_write(os.path.join(out, 'sheets' + SUF, '%d.png' % sp), [big[0][y] + gap + big[1][y] + gap + big[2][y] for y in range(len(big[0]))])
        print('compare %d: outline error %.3f, overlap %.3f, colour %.1f' % (sp, r['outline_err'], r['iou'], r['colour_diff']), flush=True)
    rows.sort(key=lambda r: -r['outline_err'])
    with open(os.path.join(out, 'report' + SUF + '.tsv'), 'w', encoding='utf-8') as f:
        f.write('species\tname\teffects\toutline_err\tiou\tonly_game\tonly_studio\tarea_game\tarea_studio\tcolour_diff\n')
        for r in rows:
            f.write('%d\t%s\t%s\t%.4f\t%.4f\t%d\t%d\t%d\t%d\t%.1f\n' % (
                r['species'], names.get(r['species'], ''), 'yes' if r['species'] in EFFECTS else '', r['outline_err'], r['iou'],
                r['only_game'], r['only_studio'], r['area_game'], r['area_studio'], r['colour_diff']))
    print('report: %s (%d species)' % (os.path.join(out, 'report' + SUF + '.tsv'), len(rows)))


# ---- the exact data check: every drawn part's material and textures, the game's log against Studio's reader ----
# The game's lines: the project's ORRE_WAZA_VIEWER_TEXLOG (docs/port/setup.md, the _TEXLOG row): what HSD bound for each
# polygon object on one frame of the real draw. Studio's: `orrestudio-cli texlog <species>` (what its reader made of the
# same model). Keyed by (part, dobj, pobj); offsets on both sides are from the DAT's data section.
def kv(tokens):
    d = {}
    for t in tokens:
        if '=' in t:
            k, v = t.split('=', 1); d[k] = v
    return d


def game_texlog(path):
    parts = {}
    for line in open(path, encoding='utf-8', errors='replace'):
        if '[waza_viewer] texlog ' not in line: continue
        tok = line.split('[waza_viewer] texlog ', 1)[1].split()
        kind, d = tok[0], kv(tok[1:])
        if 'part' not in d: continue
        key = (int(d['part']), int(d['dobj']), int(d['pobj']))
        p = parts.setdefault(key, {'tobjs': {}})
        if kind in ('pobj', 'mat', 'matpe', 'pe', 'gsmat'):
            p[kind] = d
            if kind in ('matpe', 'gsmat'): p[kind + '_none'] = 'none' in tok[1:]
        elif kind in ('tobj', 'tlut', 'tobjtev', 'tobjmtx'):
            p['tobjs'].setdefault(int(d['tobj']), {}).update({kind + '.' + k: v for k, v in d.items()})
    return parts


def studio_texlog(sp):
    env = dict(os.environ); env['PATH'] = MINGW + os.pathsep + env.get('PATH', '')
    r = subprocess.run([os.path.join(STUDIO, 'orrestudio-cli.exe'), '--disc', DISC, 'texlog', str(sp)], capture_output=True,
                       text=True, env=env)
    parts, cur = {}, None
    for line in r.stdout.splitlines():
        if line.startswith('part '):
            head, rest = line.split(':', 1)
            h = head.split()
            key = (int(h[1]), int(h[3]), int(h[5]))
            t = rest.split()
            m = {'render': int(t[t.index('render') + 1], 16), 'diffuse': t[t.index('diffuse') + 1],
                 'ambient': t[t.index('ambient') + 1], 'specular': t[t.index('specular') + 1],
                 'alpha': float(t[t.index('alpha') + 1]), 'shininess': float(t[t.index('shininess') + 1]),
                 'hidden': 'hidden' in t, 'glow': 'glow' in t, 'stages': []}
            i = t.index('pe')
            m['pe'] = None if t[i + 1] == 'none' else [int(x) for x in t[i + 1:i + 13]]
            parts[key] = cur = m
        elif line.strip().startswith('stage ') and cur is not None:
            t = line.split(':', 1)[1].split()
            g = lambda k, n=1: t[t.index(k) + n]
            cur['stages'].append({'img': int(g('image'), 16), 'fmt': int(g('format')), 'w': int(g('format', 2).split('x')[0]),
                                  'h': int(g('format', 2).split('x')[1]), 'tlut': int(g('tlut'), 16),
                                  'wrap_s': int(g('wrap')), 'wrap_t': int(g('wrap', 2)), 'flags': int(g('flags'), 16),
                                  'blend': float(g('blend')), 'repeat_s': int(g('repeat')), 'repeat_t': int(g('repeat', 2))})
    return parts


def compare_tex(game, studio):
    # returns a list of (category, text) differences
    out = []
    shown = {k: v for k, v in studio.items() if not v['hidden']}
    for k in sorted(set(shown) - set(game)):
        out.append(('drawn by Studio only', 'part %d dobj %d pobj %d' % k))
    for k in sorted(set(game) - set(studio)):
        out.append(('drawn by the game only', 'part %d dobj %d pobj %d' % k))
    for k in sorted(set(studio) & set(game)):
        s, g = studio[k], game[k]
        at = 'part %d dobj %d pobj %d' % k
        if s['hidden']: out.append(('hidden at rest in Studio, drawn by the game', at))
        mat = g.get('mat', {})
        if mat:
            mrm = int(mat.get('mrm', '0x0'), 16)
            for bit in range(32):
                b = 1 << bit
                if (mrm & b) != (s['render'] & b):
                    out.append(('render-mode bit 0x%08X %s by the game' % (b, 'added' if mrm & b else 'cleared'), at))
            for c in ('diffuse', 'ambient', 'specular'):
                if mat.get(c, '').upper() != s[c].upper():
                    out.append(('material %s colour' % c, '%s: game %s, Studio %s' % (at, mat.get(c), s[c])))
            if abs(float(mat.get('alpha', 'nan')) - s['alpha']) > 1e-4: out.append(('material alpha', '%s: game %s, Studio %.4f' % (at, mat.get('alpha'), s['alpha'])))
            if abs(float(mat.get('shininess', 'nan')) - s['shininess']) > 1e-3: out.append(('material shininess', '%s: game %s, Studio %.4f' % (at, mat.get('shininess'), s['shininess'])))
        if g.get('matpe_none') is not None and (s['pe'] is None) != g['matpe_none']:
            out.append(('own pixel-engine settings present on one side only', at))
        elif s['pe'] is not None and 'matpe' in g:
            names = ['flags', 'ref0', 'ref1', 'dst_alpha', 'type', 'src', 'dst', 'logic', 'z_comp', 'alpha_comp0', 'alpha_op', 'alpha_comp1']
            for n, sv in zip(names, s['pe']):
                gv = g['matpe'].get(n)
                if gv is not None and int(gv, 0) != sv: out.append(('pixel-engine field ' + n, '%s: game %s, Studio %d' % (at, gv, sv)))
        # the game's loaded texture list holds the material's own stages, plus any HSD or GS adds at setup (a toon ramp):
        # match the material's stages to the game's by order, among the game's textures from this model's file
        gt = [g['tobjs'][i] for i in sorted(g['tobjs'])]
        own = [t for t in gt if t.get('tobj.coordtype') != 'toon']
        for t in gt:
            if t.get('tobj.coordtype') == 'toon': out.append(('a toon texture added by the game', '%s: img %s %sx%s' % (at, t.get('tobj.img'), t.get('tobj.w'), t.get('tobj.h'))))
        if len(own) != len(s['stages']):
            out.append(('number of textures', '%s: game %d, Studio %d' % (at, len(own), len(s['stages']))))
        for i, (t, st) in enumerate(zip(own, s['stages'])):
            def chk(name, gv, sv):
                if gv != sv: out.append(('texture ' + name, '%s stage %d: game %s, Studio %s' % (at, i, gv, sv)))
            # (species whose model the game copies into a private buffer, 0x18 / 0x4A / 0x134, print no offsets: "none";
            # their pictures are then checked by format, size and the rest only)
            img = t.get('tobj.img', 'none')
            if img.startswith('0x'): chk('image', int(img, 16), st['img'])
            else: out.append(('image offsets not printed by the game (private buffer)', at))
            chk('format', int(t.get('tobj.fmt', -1)), st['fmt'])
            chk('size', (int(t.get('tobj.w', -1)), int(t.get('tobj.h', -1))), (st['w'], st['h']))
            tl = t.get('tlut.tlut', 'none')
            if tl.startswith('0x') or img.startswith('0x'): chk('palette', int(tl, 16) if tl.startswith('0x') else 0, st['tlut'])
            chk('wrap', (int(t.get('tobj.wrap_s', -1)), int(t.get('tobj.wrap_t', -1))), (st['wrap_s'], st['wrap_t']))
            chk('repeat', (int(t.get('tobj.repeat_s', -1)), int(t.get('tobj.repeat_t', -1))), (st['repeat_s'], st['repeat_t']))
            chk('flags', int(t.get('tobjtev.flags', '0x0'), 16), st['flags'])
            if abs(float(t.get('tobjtev.blending', 'nan')) - st['blend']) > 1e-4:
                out.append(('texture blending', '%s stage %d: game %s, Studio %.4f' % (at, i, t.get('tobjtev.blending'), st['blend'])))
    return out


def cmd_texcheck(args, out):
    os.makedirs(os.path.join(out, 'texdiff'), exist_ok=True)
    names = {}
    if os.path.exists(os.path.join(out, 'names.tsv')):
        for line in open(os.path.join(out, 'names.tsv'), encoding='utf-8'):
            k, v = line.rstrip('\n').split('\t', 1); names[int(k)] = v
    totals, per = {}, []
    for sp in species_list(args, out):
        cam = os.path.join(out, 'game', '%d.camera.txt' % sp)
        if not os.path.exists(cam): continue
        g = game_texlog(cam)
        if not g:
            per.append((sp, ['no texture log from the game'])); continue
        diffs = compare_tex(g, studio_texlog(sp))
        cats = sorted({c for c, _ in diffs})
        for c in cats: totals[c] = totals.get(c, 0) + 1
        with open(os.path.join(out, 'texdiff', '%d.txt' % sp), 'w', encoding='utf-8') as f:
            for c, t in diffs: f.write('%s\t%s\n' % (c, t))
        per.append((sp, cats))
        print('texcheck %d: %s' % (sp, '; '.join(cats) if cats else 'identical'), flush=True)
    with open(os.path.join(out, 'report_tex.tsv'), 'w', encoding='utf-8') as f:
        f.write('species\tname\tdifferences\n')
        for sp, cats in per: f.write('%d\t%s\t%s\n' % (sp, names.get(sp, ''), '; '.join(cats) if cats else 'identical'))
        f.write('\n# how many species show each kind of difference\n')
        for c, n in sorted(totals.items(), key=lambda x: -x[1]): f.write('%d\t%s\n' % (n, c))
    print('texture report: %s; identical: %d of %d' % (os.path.join(out, 'report_tex.tsv'), sum(1 for _, c in per if not c), len(per)))


def cmd_framing(args, out):
    os.makedirs(out, exist_ok=True)
    cli = os.path.join(STUDIO, 'orrestudio-cli.exe')
    env = dict(os.environ); env['PATH'] = MINGW + os.pathsep + env.get('PATH', '')
    r = subprocess.run([cli, '--disc', DISC, 'framing'], capture_output=True, text=True, env=env)
    open(os.path.join(out, 'framing.tsv'), 'w').write(r.stdout)
    print('framing: %d species' % len(r.stdout.splitlines()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('step', choices=['framing', 'game', 'studio', 'compare', 'texcheck'])
    ap.add_argument('--out', required=True)
    ap.add_argument('--species', default='')
    ap.add_argument('--lock', action='store_true')
    ap.add_argument('--redo', action='store_true')
    # --game-lighting: Studio renders with in-game lighting into studio_lit/, compared into report_lit.tsv / sheets_lit/
    # (the same game shots, a strict colour test once lighting lands; release step 2)
    ap.add_argument('--game-lighting', action='store_true')
    # --shiny: every step on the shiny models (the 35 separate rare_ models and the colour-block recolour of the rest);
    # give it its own --out folder (with a copy of framing.tsv and names.tsv)
    ap.add_argument('--shiny', action='store_true')
    args = ap.parse_args()
    global SUF, SHINY
    SUF = '_lit' if args.game_lighting else ''
    SHINY = args.shiny
    {'framing': cmd_framing, 'game': cmd_game, 'studio': cmd_studio, 'compare': cmd_compare,
     'texcheck': cmd_texcheck}[args.step](args, os.path.abspath(args.out))


if __name__ == '__main__':
    main()
