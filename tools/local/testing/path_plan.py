"""Plan a walking path through a room from its collision (testing lane).

    python -I tools/local/testing/path_plan.py <room collision .obj> <start x,z> <goal x,z> [--cell 4] [--png out.png]

Reads the room's collision OBJ (extract.py's maps stage: objNN_floor = walkable floor, objNN_walls_player = walls for the
player; docs/engine/collision.md), rasterises it onto a grid (a cell is walkable when its centre lies on a floor
triangle seen from above; cells a player wall crosses, plus a one-cell margin, are blocked), runs A* (8 directions) and
simplifies the path to straight segments that stay on walkable cells. Prints the waypoints as "x,z;x,z;..." (the format
dolphin_goto.py's GOTO_TO takes). Height is ignored (stairs and ramps are floor; rooms with floors over floors would
need layers). --png draws the grid, the path and the waypoints for checking.
"""
import argparse, heapq, math, sys

STEP = 3.0   # the largest height change between neighbouring cells the player can walk (ramps and stairs are sloped floor)


def load(path):
    V = []; tris = {"floor": [], "wall": []}; cur = ""
    for l in open(path):
        if l.startswith("o "):
            cur = l.split()[1]
        elif l.startswith("v "):
            V.append(tuple(map(float, l.split()[1:4])))
        elif l.startswith("f "):
            t = [V[int(x.split("/")[0]) - 1] for x in l.split()[1:4]]
            if cur.endswith("_floor") or "_floor_" in cur:
                tris["floor"].append(t)
            elif "walls_player" in cur:
                tris["wall"].append(t)
    return tris


def inside(px, pz, t):
    (ax, _, az), (bx, _, bz), (cx, _, cz) = t
    d1 = (px - bx) * (az - bz) - (ax - bx) * (pz - bz)
    d2 = (px - cx) * (bz - cz) - (bx - cx) * (pz - cz)
    d3 = (px - ax) * (cz - az) - (cx - ax) * (pz - az)
    neg = d1 < 0 or d2 < 0 or d3 < 0; pos = d1 > 0 or d2 > 0 or d3 > 0
    return not (neg and pos)


def build(tris, cell, margin=1):
    pts = [p for t in tris["floor"] for p in t]
    x0, x1 = min(p[0] for p in pts) - cell, max(p[0] for p in pts) + cell
    z0, z1 = min(p[2] for p in pts) - cell, max(p[2] for p in pts) + cell
    W, H = int((x1 - x0) / cell) + 1, int((z1 - z0) / cell) + 1
    walk = [[False] * W for _ in range(H)]
    height = [[None] * W for _ in range(H)]          # the highest floor at each cell centre (the top surface)
    for t in tris["floor"]:
        xs = [p[0] for p in t]; zs = [p[2] for p in t]
        (ax, ay, az), (bx, by, bz), (cx, cy, cz) = t
        den = (bz - cz) * (ax - cx) + (cx - bx) * (az - cz)
        for j in range(max(0, int((min(zs) - z0) / cell)), min(H, int((max(zs) - z0) / cell) + 2)):
            for i in range(max(0, int((min(xs) - x0) / cell)), min(W, int((max(xs) - x0) / cell) + 2)):
                px, pz = x0 + (i + .5) * cell, z0 + (j + .5) * cell
                if inside(px, pz, t):
                    walk[j][i] = True
                    if abs(den) > 1e-9:
                        l1 = ((bz - cz) * (px - cx) + (cx - bx) * (pz - cz)) / den
                        l2 = ((cz - az) * (px - cx) + (ax - cx) * (pz - cz)) / den
                        y = l1 * ay + l2 * by + (1 - l1 - l2) * cy
                    else:
                        y = max(ay, by, cy)
                    if height[j][i] is None or y > height[j][i]:
                        height[j][i] = y
    build.height = height
    block = [[False] * W for _ in range(H)]
    for t in tris["wall"]:
        for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            n = max(1, int(math.hypot(b[0] - a[0], b[2] - a[2]) / (cell / 3)))
            for k in range(n + 1):
                x = a[0] + (b[0] - a[0]) * k / n; z = a[2] + (b[2] - a[2]) * k / n
                i, j = int((x - x0) / cell), int((z - z0) / cell)
                for dj in range(-margin, margin + 1):
                    for di in range(-margin, margin + 1):
                        if 0 <= j + dj < H and 0 <= i + di < W:
                            block[j + dj][i + di] = True
    ok = [[walk[j][i] and not block[j][i] for i in range(W)] for j in range(H)]
    return ok, x0, z0, W, H


def astar(ok, s, g, W, H):
    def h(a): return math.hypot(a[0] - g[0], a[1] - g[1])
    openq = [(h(s), 0.0, s)]; came = {s: None}; cost = {s: 0.0}
    while openq:
        _, c, cur = heapq.heappop(openq)
        if cur == g:
            path = []
            while cur: path.append(cur); cur = came[cur]
            return path[::-1]
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
            n = (cur[0] + di, cur[1] + dj)
            if not (0 <= n[0] < W and 0 <= n[1] < H) or not ok[n[1]][n[0]]:
                continue
            hgt = getattr(build, "height", None)
            if hgt and hgt[n[1]][n[0]] is not None and hgt[cur[1]][cur[0]] is not None                     and abs(hgt[n[1]][n[0]] - hgt[cur[1]][cur[0]]) > STEP:
                continue                                  # a ledge or a drop (into water): not walkable
            if di and dj and not (ok[cur[1]][cur[0] + di] and ok[cur[1] + dj][cur[0]]):
                continue
            nc = c + math.hypot(di, dj)
            if nc < cost.get(n, 1e18):
                cost[n] = nc; came[n] = cur; heapq.heappush(openq, (nc + h(n), nc, n))
    return None


def nearest_ok(ok, p, W, H):
    best = None
    for j in range(H):
        for i in range(W):
            if ok[j][i]:
                d = (i - p[0]) ** 2 + (j - p[1]) ** 2
                if best is None or d < best[0]: best = (d, (i, j))
    return best[1]


def clear(ok, a, b):
    hg = getattr(build, "height", None)
    n = int(max(abs(b[0] - a[0]), abs(b[1] - a[1])) * 2) + 1
    prev = None
    for k in range(n + 1):
        i = a[0] + (b[0] - a[0]) * k / n; j = a[1] + (b[1] - a[1]) * k / n
        for ii, jj in ((math.floor(i), math.floor(j)), (math.ceil(i), math.ceil(j))):
            if not ok[jj][ii]: return False
            if hg and hg[jj][ii] is not None:
                if prev is not None and abs(hg[jj][ii] - prev) > STEP: return False
                prev = hg[jj][ii]
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("obj"); ap.add_argument("start"); ap.add_argument("goal")
    ap.add_argument("--cell", type=float, default=2.0); ap.add_argument("--png")
    ap.add_argument("--npcs", help="npc_placements_decoded.csv: people block movement too (a radius around each)")
    ap.add_argument("--room", help="the room archive name for --npcs (e.g. M1_out)")
    ap.add_argument("--npc-radius", type=float, default=7.0)
    ap.add_argument("--margin", type=int, default=0, help="cells blocked on each side of a wall")
    a = ap.parse_args()
    tris = load(a.obj)
    ok, x0, z0, W, H = build(tris, a.cell, a.margin)
    if a.npcs and a.room:
        import csv
        for r in csv.DictReader(open(a.npcs, encoding="utf-8")):
            if r["room"] != a.room: continue
            nx, nz = float(r["x"]), float(r["z"]); rr = a.npc_radius
            for j in range(max(0, int((nz - rr - z0) / a.cell)), min(H, int((nz + rr - z0) / a.cell) + 1)):
                for i in range(max(0, int((nx - rr - x0) / a.cell)), min(W, int((nx + rr - x0) / a.cell) + 1)):
                    if math.hypot(x0 + (i + .5) * a.cell - nx, z0 + (j + .5) * a.cell - nz) <= rr:
                        ok[j][i] = False
    to_cell = lambda p: (int((p[0] - x0) / a.cell), int((p[1] - z0) / a.cell))
    to_world = lambda c: (round(x0 + (c[0] + .5) * a.cell, 1), round(z0 + (c[1] + .5) * a.cell, 1))
    s = tuple(map(float, a.start.split(","))); g = tuple(map(float, a.goal.split(",")))
    sc, gc = to_cell(s), to_cell(g)
    if not (0 <= sc[0] < W and 0 <= sc[1] < H and ok[sc[1]][sc[0]]): sc = nearest_ok(ok, sc, W, H)
    # the goal may sit behind a closed door (its collision blocks until it opens): plan to the nearest cell reachable
    # from the start instead; the walker then pushes on to the real goal, which opens the door by touch
    seen = {sc}; stack = [sc]
    while stack:
        cx, cz = stack.pop()
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):   # 4-way, so A* can always follow it
            n = (cx + di, cz + dj)
            if 0 <= n[0] < W and 0 <= n[1] < H and ok[n[1]][n[0]] and n not in seen:
                hg = build.height
                if hg[n[1]][n[0]] is not None and hg[cz][cx] is not None and abs(hg[n[1]][n[0]] - hg[cz][cx]) > STEP:
                    continue
                seen.add(n); stack.append(n)
    if gc not in seen:
        gc = min(seen, key=lambda c: (c[0] - gc[0]) ** 2 + (c[1] - gc[1]) ** 2)
        print(f"goal not reachable; planning to {to_world(gc)}", file=sys.stderr)
    path = astar(ok, sc, gc, W, H)
    if not path:
        print("no path", file=sys.stderr); sys.exit(1)
    pts = [path[0]]; k = 0
    while k < len(path) - 1:
        j = len(path) - 1
        while j > k + 1 and not clear(ok, path[k], path[j]): j -= 1
        pts.append(path[j]); k = j
    way = [to_world(c) for c in pts[1:]] + [g]
    print(";".join(f"{x},{z}" for x, z in way))
    if a.png:
        sys.path.insert(0, __import__("os").path.dirname(__file__)); from framediff import write_png
        img = [[(40, 40, 40) if not ok[j][i] else (200, 200, 200) for i in range(W)] for j in range(H)]
        for c in path: img[c[1]][c[0]] = (60, 120, 255)
        for c in pts: img[c[1]][c[0]] = (255, 40, 40)
        write_png(a.png, W, H, img)


def plan(obj, start, goal, cell=2.0, margin=2, npcs=None, room=None, npc_radius=7.0, people=None):
    """Waypoints [(x, z), ...] from start to goal (same rules as the command line); None when there is no path."""
    import csv
    tris = load(obj)
    ok, x0, z0, W, H = build(tris, cell, margin)
    blockers = list(people or [])                 # live positions (from RAM) are better than the room's placement list,
    if not people and npcs and room:              # which also holds people not in the scene (yet / any more)
        blockers = [(float(r["x"]), float(r["z"])) for r in csv.DictReader(open(npcs, encoding="utf-8")) if r["room"] == room]
    if blockers:
        for nx, nz in blockers:
            for j in range(max(0, int((nz - npc_radius - z0) / cell)), min(H, int((nz + npc_radius - z0) / cell) + 1)):
                for i in range(max(0, int((nx - npc_radius - x0) / cell)), min(W, int((nx + npc_radius - x0) / cell) + 1)):
                    if math.hypot(x0 + (i + .5) * cell - nx, z0 + (j + .5) * cell - nz) <= npc_radius:
                        ok[j][i] = False
    to_cell = lambda p: (int((p[0] - x0) / cell), int((p[1] - z0) / cell))
    to_world = lambda c: (round(x0 + (c[0] + .5) * cell, 1), round(z0 + (c[1] + .5) * cell, 1))
    sc, gc = to_cell(start), to_cell(goal)
    if not (0 <= sc[0] < W and 0 <= sc[1] < H and ok[sc[1]][sc[0]]): sc = nearest_ok(ok, sc, W, H)
    seen = {sc}; stack = [sc]
    while stack:
        cx, cz = stack.pop()
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (cx + di, cz + dj)
            if 0 <= n[0] < W and 0 <= n[1] < H and ok[n[1]][n[0]] and n not in seen:
                hg = build.height
                if hg[n[1]][n[0]] is not None and hg[cz][cx] is not None and abs(hg[n[1]][n[0]] - hg[cz][cx]) > STEP:
                    continue
                seen.add(n); stack.append(n)
    if gc not in seen:
        gc = min(seen, key=lambda c: (c[0] - gc[0]) ** 2 + (c[1] - gc[1]) ** 2)
    path = astar(ok, sc, gc, W, H)
    if not path: return None
    pts = [path[0]]; k = 0
    while k < len(path) - 1:
        j = len(path) - 1
        while j > k + 1 and not clear(ok, path[k], path[j]): j -= 1
        pts.append(path[j]); k = j
    return [to_world(c) for c in pts[1:]] + [tuple(goal)]


if __name__ == "__main__":
    main()
