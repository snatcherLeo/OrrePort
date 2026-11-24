"""Decode a flag dump (dolphin_flags.py JSON, or the PC's ORRE_DUMP_FLAGS flags_end.bin) and report story progress (testing lane).

    python -I tools/local/testing/flag_report.py <dump.flags.json> <disc image> [docs/testing/route.json]

Bit positions as the game packs them (GSflagInitBitPos; colo/savefile.flag_layout). Prints flags set per class, the
furthest story-chain flag set (the chain is story order, flags.md), and for each route step whether its flags are set.
"""
import json, os, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.dirname(HERE))
from colo.disc import Disc
from colo.tables import common_rel, table_rows
from colo import world
from colo.savefile import flag_layout

if sys.argv[1].endswith(".bin"):
    # the PC's ORRE_DUMP_FLAGS file (B, 2026-10-16): big-endian u32 words, class 1 (8), class 2 (32), class 3 (384)
    raw = open(sys.argv[1], "rb").read(); w = list(struct.unpack(f">{len(raw) // 4}I", raw))
    dump = {"state": os.path.basename(sys.argv[1]), "classes": {"1": w[:8], "2": w[8:40], "3": w[40:424]}}
else:
    dump = json.load(open(sys.argv[1]))
rel = common_rel(Disc(sys.argv[2]))
defs = table_rows(rel, 0x111e04, 0x14a6c0, 8)
cfg = world.common_table(rel, 0x80478F98); cur = struct.unpack(">h", rel[cfg + 4:cfg + 6])[0]; chain = []
while 0 <= cur < len(defs) and len(chain) <= len(defs):
    chain.append(cur); cur = struct.unpack(">h", defs[cur][6:8])[0]
words = {int(k): v for k, v in dump["classes"].items()}
vals = {}
for i, (cls, w, pos) in enumerate(flag_layout(defs)):
    if cls not in words or pos is None: continue
    buf, wi, bp = words[cls], pos >> 5, pos & 31
    if wi >= len(buf): continue
    v = buf[wi] >> bp
    if w > 1 and bp and wi + 1 < len(buf): v |= buf[wi + 1] << (32 - bp)
    vals[i] = v & ((1 << w) - 1)
setf = {f for f, v in vals.items() if v}
# Not story progress (2026-10-16): flag 0 is the "no flag" id, and the 607 character-name "heard" flags sit early in
# the chain outside story order (set when a name is first heard, e.g. 2910 for Willie at his battle's start).
HEARD = {c["flag"] for c in world.character_names(rel) if c["flag"]}
story = {f for f in setf if f != 0 and f not in HEARD}
pos = {f: k for k, f in enumerate(chain)}
far = max((pos[f] for f in story if f in pos), default=-1)
print(f"{dump.get('state')}: classes {sorted(words)}; flags set {len(setf)} "
      f"(class 1: {sum(1 for f in setf if defs[f][0] >> 6 == 1)}, 2: {sum(1 for f in setf if defs[f][0] >> 6 == 2)}, "
      f"3: {sum(1 for f in setf if defs[f][0] >> 6 == 3)}); story flags {len(story)} (without flag 0 and {len(setf & HEARD)} "
      f"heard-name flags); furthest chain position {far} of {len(chain)}")
print("  set:", " ".join(f"{f}(c{defs[f][0] >> 6},pos {pos.get(f, '-')})={vals[f]}" for f in sorted(setf, key=lambda f: pos.get(f, 1e9))))
if len(sys.argv) > 3:
    steps = json.load(open(sys.argv[3], encoding="utf-8"))["steps"]
    # Route progress (2026-10-16: a single furthest chain flag can overstate, e.g. 1005 is a late "around town with
    # Rui" marker): the highest step whose own flags are all set, and the main path completed without a gap.
    need = lambda st: [f for f in st["flags"] if f.get("required", True)]   # skip the project's no-setter markers
    full = [st["step"] for st in steps if need(st) and all(vals.get(f["id"]) for f in need(st))]
    contiguous = 0
    for st in steps:
        if not st.get("main_path") or not need(st):
            continue
        if all(vals.get(f["id"]) for f in need(st)):
            contiguous = st["step"]
        else:
            break
    print(f"  route: highest step with all its flags set {max(full) if full else 0}; main path complete through step "
          f"{contiguous} of {len(steps)}")
    for s in steps:
        ids = [f["id"] for f in s["flags"]]
        print(f"  step {s['step']:2} {(s.get('id') or s['room'].get('archive') or ''):24} " + " ".join(f"{i}={vals.get(i, '?')}" for i in ids))
