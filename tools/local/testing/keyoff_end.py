"""port-gaps 28 (2026-11-01; landed in tools/local/testing/ by D): per sample, the console's key-off-to-end
times from the voices probe (voices.csv; key-off = the first row of a voice's life whose adsr state is 4), and the PC's
from ORRE_SOUND_LOG's "[sndvoice]" start / release / end lines (end lines carry why= since 2026-11-01). Times in
retraces (PC: mixer seconds x 59.94). The PC run needs ORRE_SOUND_LOG=1; the "[sndvoice] ... budget:" summary lines are
skipped on purpose (the regexes don't match them).

    python -I keyoff_end.py <voices.csv> <pc log> [samples...]
"""
import collections
import csv
import re
import statistics
import sys

voices_csv, pc_log = sys.argv[1], sys.argv[2]
want = {int(s) for s in sys.argv[3:]} or None

con = collections.defaultdict(list)          # smp -> [(keyoff->end, env at end, adsr mode)]
life = {}                                     # voice -> [smp, start retrace, keyoff retrace or None]
for r in csv.DictReader(open(voices_csv, encoding="utf-8")):
    v, t, smp = r["voice"], int(r["retrace"]), int(r["smp"])
    if r["what"] == "start":
        life[v] = [smp, t, None]
    elif v in life:
        l = life[v]
        if l[2] is None and r["adsr"].endswith(":4"):
            l[2] = t
        if r["what"] == "end":
            if l[2] is not None:
                con[l[0]].append((t - l[2], int(r["env"]), r["adsr"].split(":")[0]))
            del life[v]

START = re.compile(r"\[sndvoice\] t=([\d.]+) voice=(\d+) smp=(\d+) studio")
REL = re.compile(r"\[sndvoice\] t=([\d.]+) release voice=(\d+) smp=(\d+)")
END = re.compile(r"\[sndvoice\] t=([\d.]+) end voice=(\d+) smp=(\d+)(?: why=(\w+) env=0x([0-9A-F]+))?")
pc = collections.defaultdict(list)           # smp -> [(keyoff->end, why, env)]
why_all = collections.defaultdict(collections.Counter)
restarts = collections.Counter()             # smp -> ends that were restarts in place (left out of the medians)
RESTART_S = 0.020   # an end followed by a start on the same voice this soon is a restart in place (2026-11-01,
                    # after the project's 625 finding): the console voice never goes idle there, so voices.csv has no end
plife = {}
pend = {}                                     # voice -> (end t, smp, release t, why, env), held until the next start


def settle(v):
    t, smp, rel, why, env = pend.pop(v)
    why_all[smp][why] += 1
    if rel is not None:
        pc[smp].append(((t - rel) * 59.94, why, env))


for line in open(pc_log, encoding="utf-8", errors="replace"):
    m = START.match(line)
    if m:
        v = m.group(2)
        if v in pend:
            if float(m.group(1)) - pend[v][0] <= RESTART_S:
                restarts[pend.pop(v)[1]] += 1
            else:
                settle(v)
        plife[v] = [int(m.group(3)), None]
        continue
    m = REL.match(line)
    if m and m.group(2) in plife and plife[m.group(2)][1] is None:
        plife[m.group(2)][1] = float(m.group(1))
        continue
    m = END.match(line)
    if m and m.group(2) in plife:
        v = m.group(2)
        if v in pend:
            settle(v)
        smp, rel = plife.pop(v)
        pend[v] = (float(m.group(1)), smp, rel, m.group(4) or "?", int(m.group(5) or "0", 16))
for v in list(pend):
    settle(v)

for smp in sorted(set(con) | set(pc)):
    if want and smp not in want:
        continue
    c, p = con.get(smp, []), pc.get(smp, [])
    if not c and not p:
        continue
    cm = statistics.median(x[0] for x in c) if c else None
    pm = statistics.median(x[0] for x in p) if p else None
    chigh = sum(1 for x in c if x[1] > 0) if c else 0
    print(f"smp {smp}: console n={len(c)} median {cm} (ended with env>0: {chigh}) | PC n={len(p)} median "
          f"{pm if pm is None else round(pm, 1)} | PC ends after key-off by why: "
          f"{dict(collections.Counter(x[1] for x in p))} | all PC ends: {dict(why_all[smp])} | restarts in place "
          f"(left out): {restarts[smp]}")
