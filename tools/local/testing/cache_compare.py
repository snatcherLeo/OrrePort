"""The fsys ARAM cache, console vs PC (2026-10-25). Turns each side's cache rows (dolphin_playrec.py's cache probe,
cache.csv; the PC's ORRE_CACHE_LOG=1 "[cache]" lines) into the ordered list of events "+ handle:key1:key2:size" (an
entry added) and "- ..." (evicted), and prints the first place the two lists differ, with the events around it and
each side's frame. Order only, no clock alignment, so it works across rooms whose timing differs.

    python -I tools/local/testing/cache_compare.py <cache.csv> <pc log> [context]
"""
import re
import sys


def events(rows):
    """rows: (frame, "node:handle:k1:k2:size:aram ...") -> [(frame, "+"/"-", "handle:k1:k2:size")]"""
    out, prev = [], set()
    for frame, row in rows:
        now = set()
        for part in row.split():
            a = part.split(":")
            if len(a) >= 5:
                now.add(":".join(a[1:5]))
        for k in sorted(now - prev):
            out.append((frame, "+", k))
        for k in sorted(prev - now):
            out.append((frame, "-", k))
        prev = now
    return out


def main():
    con, log = sys.argv[1], sys.argv[2]
    ctx = int(sys.argv[3]) if len(sys.argv) > 3 else 4
    c = []
    with open(con) as f:
        for line in f:
            if line.startswith("#") or line.startswith("frame"):
                continue
            parts = line.rstrip("\n").split(",", 2)
            c.append((int(parts[0]), parts[2] if len(parts) > 2 else ""))
    p = []
    for line in open(log, errors="replace"):
        m = re.match(r"\[cache\] (\d+),\d+,(.*)", line)
        if m:
            p.append((int(m.group(1)), m.group(2)))
    ec, ep = events(c), events(p)
    print(f"console {len(ec)} events, PC {len(ep)} events")
    k = next((i for i, (a, b) in enumerate(zip(ec, ep)) if a[1:] != b[1:]), None)
    if k is None:
        print("the same events in the same order" + ("" if len(ec) == len(ep) else " up to the shorter list's end"))
        return
    print(f"first difference at event {k}:")
    for i in range(max(0, k - ctx), min(max(len(ec), len(ep)), k + ctx + 1)):
        a = ec[i] if i < len(ec) else None
        b = ep[i] if i < len(ep) else None
        fa = f"{a[0]:7d} {a[1]} {a[2]:32s}" if a else " " * 42
        fb = f"{b[0]:7d} {b[1]} {b[2]}" if b else ""
        print(f"{'>' if i == k else ' '} {i:4d}  console {fa}  PC {fb}")


if __name__ == "__main__":
    main()
