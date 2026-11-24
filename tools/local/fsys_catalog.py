"""Turn fsys_list.py --tsv output into the bulk markdown tables of docs/engine/fsys-catalog.md.
Usage: python -I fsys_list.py <image.iso> --tsv > list.tsv
       python -I fsys_catalog.py list.tsv <image.iso> > bulk.md
Names, type codes and counts only (no game data). The image is read for GSfsys.toc, to flag
archives that the toc does not name (the game cannot request those)."""
import collections, re, struct, sys

KIND = {0: "data", 1: "map", 2: "model", 3: "col", 4: "bgm", 5: "msg", 6: "font", 7: "script",
        9: "tex", 10: "ptcl", 12: "cam", 14: "rel?", 15: "pkx", 16: "wzx", 17: "gfl", 18: "ptcl-nl",
        19: "relmod"}
ORDER = [14, 7, 5, 6, 1, 3, 2, 9, 10, 12, 4, 0, 15, 16, 17, 18, 19]

arch = collections.OrderedDict()
for line in open(sys.argv[1], encoding="ascii"):
    f = line.rstrip("\n").split("\t")
    arch.setdefault(f[0][:-5], {"id": f[1], "rows": []})["rows"].append(f)

iso = open(sys.argv[2], "rb")
def rd(o, n): iso.seek(o); return iso.read(n)
fo, fs = struct.unpack(">II", rd(0x424, 8)); fst = rd(fo, fs)
n = struct.unpack(">I", fst[8:12])[0]; st = fst[n * 12:]
toc = None
for i in range(1, n):
    w0, a, b = struct.unpack(">III", fst[i * 12:i * 12 + 12])
    if st[w0 & 0xFFFFFF:].startswith(b"GSfsys.toc\0"): toc = rd(a, b)
cnt, tab = struct.unpack(">I", toc[8:12])[0], struct.unpack(">I", toc[0x10:0x14])[0]
tocnames = set()
for k in range(cnt):
    i, o = struct.unpack(">II", toc[tab + 8 * k:tab + 8 * k + 8])
    tocnames.add(toc[o:toc.index(b"\0", o)].decode())

def summary(rows):
    if rows[0][4] == "-1": return "(empty)"
    c = collections.Counter(int(r[4]) for r in rows)
    return ", ".join(f"{KIND.get(t, t)}{'' if c[t] == 1 else ' x' + str(c[t])}" for t in ORDER if c[t])

def others(name, rows):
    return " ".join(sorted({r[3] for r in rows if r[3] != name and int(r[4]) in (2,)}))

maps = collections.defaultdict(list)
for name, a in arch.items():
    m = re.match(r"([A-Z]\d)_", name)
    if m: maps[m.group(1)].append(name)

print("### Map archives by area prefix (generated)\n")
print("Kinds: rel? = type 14, script, msg, font, map (model), col (collision), model (type 2 object/NPC")
print("models, listed in the last column), tex, ptcl, cam. `_bf` = the battle-field version of a room")
print("(see the notes above). Archive ids are the FSYS header's +0x08 value.\n")
for pre in sorted(maps):
    print(f"#### {pre} ({len(maps[pre])} archives)\n")
    print("| Archive | Id | Contents | Object / NPC models |")
    print("|---|---|---|---|")
    for name in sorted(maps[pre]):
        a = arch[name]
        print(f"| {name} | {a['id']} | {summary(a['rows'])} | {others(name, a['rows'])} |")
    print()

pkx = sorted(n[4:] for n in arch if n.startswith("pkx_"))
trainer = [p for p in pkx if re.search(r"_[a-z]?\d+$|_[mf]_|_[a-d]$", p)]
species = [p for p in pkx if p not in trainer]
print(f"### PKX archives (generated): {len(pkx)}, one type-15 entry each\n")
print(f"**Names without a numbered suffix ({len(species)}; species, romanized Japanese, guess):** "
      + " ".join(species) + "\n")
print(f"**Names with a numbered or gender suffix ({len(trainer)}; trainer / character battle "
      "models, guess):** " + " ".join(trainer) + "\n")

wzx = sorted(n[4:] for n in arch if n.startswith("wzx_"))
suf = collections.Counter((re.search(r"_([a-z]+\d*)$", w) or re.search(r"(.*)", w)).group(1) for w in wzx)
print(f"### WZX archives (generated): {len(wzx)}, one type-16 entry each\n")
print("Suffix counts: " + ", ".join(f"`_{s}` {c}" for s, c in suf.most_common(12)) + ".\n")
empty = sorted(n for n, a in arch.items() if a["rows"][0][4] == "-1")
print(f"### Empty archives (generated): {len(empty)} with no entries\n")
print(" ".join(f"{n} ({'in toc' if n in tocnames else 'not in toc'})" for n in empty) + "\n")
orphan = sorted(n for n in arch if n not in tocnames)
print(f"**Not named in GSfsys.toc ({len(orphan)}), so the game cannot request them by id:** "
      + " ".join(orphan) + "\n")
