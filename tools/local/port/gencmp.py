"""Compare build-pc/generated with a saved copy after a data rename batch (read-only).

2026-11-08 (the data batches' gate). Maps the batch's new data names back to their placeholders (also inside
pcdata's derived names `pcrec_<name>` / `pcff_<name>`) and compares each generated file with the copy saved before
the batch. Prints per file: same / same-after-map (identical once names map back) / same-lines (the same lines in
another order) / DIFF, and a final "gencmp: N bad". Expect same or same-after-map everywhere (pcdata sorts with
datanames.sort_key); same-lines is fine for declaration order only (check the PC exe with maskdis.py as well).

Usage (from the repo root): python -I tools/local/port/gencmp.py <before dir> <after dir> <batch_id>
"""
import csv
import os
import re
import sys

before, after, batch = sys.argv[1:4]
rows = [r for r in csv.DictReader(open('docs/data_renames.csv', encoding='utf-8')) if r['batch_id'] == batch]
mp = {r['new_name']: r['old_name'] for r in rows}
pat = re.compile(r'\b(?:pcrec_|pcff_)?(' + '|'.join(sorted(map(re.escape, mp), key=len, reverse=True)) + r')\b') if mp else None
bad = 0
for name in sorted(os.listdir(before)):
    a = open(os.path.join(before, name), 'rb').read().decode('utf-8', 'replace').replace('\r\n', '\n')
    p = os.path.join(after, name)
    if not os.path.exists(p):
        print('MISSING', name)
        bad += 1
        continue
    b = open(p, 'rb').read().decode('utf-8', 'replace').replace('\r\n', '\n')
    if a == b:
        print('same', name)
        continue
    bm = pat.sub(lambda m: m.group(0)[:len(m.group(0)) - len(m.group(1))] + mp[m.group(1)], b) if pat else b
    if bm == a:
        print('same-after-map', name)
    elif sorted(bm.split('\n')) == sorted(a.split('\n')):
        print('same-lines', name)
    else:
        print('DIFF', name)
        bad += 1
print('gencmp: %d bad' % bad)
