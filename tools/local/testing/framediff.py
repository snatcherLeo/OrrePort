"""PC frame vs Dolphin frame: side-by-side + highlighted difference PNG and a difference score (testing lane).

For the visual-check sessions (docs/testing/visual-session.md; snatcherLeo's rule: each item gets this automated comparison,
a first reading of both images, and snatcherLeo's verdict by eye; verified only when the two agree).

    python -I tools/local/testing/framediff.py <pc.png> <dolphin.png> <out.png> [--threshold 24] [--align 4] [--json f]

- Both frames are resampled (bilinear) to the PC frame's size first: the PC dumps 640x480, Dolphin's screenshots are
  640x511 (its XFB height), same width and aspect.
- `--align N` tries shifts of up to N pixels and keeps the best one (a frame that is off by a few pixels is not a
  difference worth flagging).
- The output image has three panels: PC | Dolphin | difference (Dolphin in grey, pixels whose largest channel
  difference exceeds the threshold in red, and 16-pixel blocks with more than 5 % such pixels outlined in yellow).
- Printed score (and `--json`): mean absolute difference (0-255), percent of pixels over the threshold, the shift
  used, and the ten worst 16-pixel blocks (x, y, percent). Standard library only.
"""
import argparse, json, os, struct, sys, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
from colo.png import read_png  # noqa: E402


def load(path):
    w, h, c, rows = read_png(open(path, "rb").read())
    px = []
    for r in rows:
        line = []
        for x in range(w):
            p = r[x * c:(x + 1) * c]
            line.append((p[0], p[0], p[0]) if c < 3 else (p[0], p[1], p[2]))
        px.append(line)
    return w, h, px


def resample(img, w2, h2):
    w, h, px = img
    if (w, h) == (w2, h2):
        return img
    out = []
    for y in range(h2):
        fy = (y + 0.5) * h / h2 - 0.5
        y0 = max(0, min(h - 1, int(fy))); y1 = min(h - 1, y0 + 1); ty = min(1, max(0, fy - y0))
        line = []
        for x in range(w2):
            fx = (x + 0.5) * w / w2 - 0.5
            x0 = max(0, min(w - 1, int(fx))); x1 = min(w - 1, x0 + 1); tx = min(1, max(0, fx - x0))
            a, b, c, d = px[y0][x0], px[y0][x1], px[y1][x0], px[y1][x1]
            line.append(tuple(int((a[k] * (1 - tx) + b[k] * tx) * (1 - ty) + (c[k] * (1 - tx) + d[k] * tx) * ty + 0.5)
                              for k in range(3)))
        out.append(line)
    return w2, h2, out


def diff_at(a, b, dx, dy, step=1):
    """Mean of the largest channel difference over the overlap, comparing a[y][x] with b[y+dy][x+dx]."""
    w, h, pa = a; _, _, pb = b
    tot = n = 0
    for y in range(max(0, -dy), min(h, h - dy), step):
        ra, rb = pa[y], pb[y + dy]
        for x in range(max(0, -dx), min(w, w - dx), step):
            p, q = ra[x], rb[x + dx]
            tot += max(abs(p[0] - q[0]), abs(p[1] - q[1]), abs(p[2] - q[2])); n += 1
    return tot / max(n, 1)


def write_png(path, w, h, px):
    raw = b"".join(b"\x00" + bytes(v for p in row for v in p) for row in px)
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as g:
        g.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pc"); ap.add_argument("dolphin"); ap.add_argument("out")
    ap.add_argument("--threshold", type=int, default=24)
    ap.add_argument("--align", type=int, default=4)
    ap.add_argument("--json")
    a = ap.parse_args()
    pc = load(a.pc)
    dol = resample(load(a.dolphin), pc[0], pc[1])
    w, h = pc[0], pc[1]
    best = (diff_at(pc, dol, 0, 0, 4), 0, 0)
    for dy in range(-a.align, a.align + 1):
        for dx in range(-a.align, a.align + 1):
            if dx or dy:
                best = min(best, (diff_at(pc, dol, dx, dy, 4), dx, dy))
    _, dx, dy = best
    P, D = pc[2], dol[2]
    over = 0; tot = 0; hl = []; blocks = {}
    for y in range(h):
        line = []
        for x in range(w):
            yy, xx = y + dy, x + dx
            q = D[yy][xx] if 0 <= yy < h and 0 <= xx < w else (0, 0, 0)
            p = P[y][x]
            m = max(abs(p[0] - q[0]), abs(p[1] - q[1]), abs(p[2] - q[2])); tot += m
            g = (q[0] * 30 + q[1] * 59 + q[2] * 11) // 100 // 2 + 40
            if m > a.threshold:
                over += 1; line.append((255, 40, 40))
                k = (x // 16, y // 16); blocks[k] = blocks.get(k, 0) + 1
            else:
                line.append((g, g, g))
        hl.append(line)
    flagged = {k: v for k, v in blocks.items() if v > 16 * 16 * 0.05}
    for (bx, by) in flagged:                      # yellow outline around each flagged block
        for i in range(16):
            for (x, y) in ((bx * 16 + i, by * 16), (bx * 16 + i, by * 16 + 15), (bx * 16, by * 16 + i), (bx * 16 + 15, by * 16 + i)):
                if x < w and y < h:
                    hl[y][x] = (255, 220, 0)
    gap = 8
    W = w * 3 + gap * 2
    rows = []
    for y in range(h):
        rows.append(P[y] + [(255, 255, 255)] * gap + D[y] + [(255, 255, 255)] * gap + hl[y])
    write_png(a.out, W, h, rows)
    worst = sorted(blocks.items(), key=lambda kv: -kv[1])[:10]
    res = {"pc": a.pc, "dolphin": a.dolphin, "size": [w, h], "shift": [dx, dy],
           "mean_abs_diff": round(tot / (w * h), 2), "percent_over_threshold": round(100 * over / (w * h), 2),
           "threshold": a.threshold, "flagged_blocks": len(flagged),
           "worst_blocks": [{"x": k[0] * 16, "y": k[1] * 16, "percent": round(100 * v / 256, 1)} for k, v in worst]}
    print(json.dumps(res))
    if a.json:
        json.dump(res, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
