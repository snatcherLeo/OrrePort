"""Generate the PC version of every room module the build doesn't decompile (OrrePort; port-gaps.md section 8).

On the GameCube a room loads its REL module, Relocate relocates it and its `_prolog` fills main-program slots with the
addresses of the module's data. On the PC the module code can't run, and for 145 modules there is no C. This tool
turns each such module (read from the disc extraction, `extracted/**/*.rel`) into C, written only to the untracked
`build-pc/generated/pc_rel_modules.c` (marker line; never committed):
  - its data sections as byte arrays, **kept big-endian, bit-exact** (the room record types aren't settled yet, so
    every byte is listed as unsure in `pc_rel_unsure.txt`; port-gaps.md 6.2);
  - pointer words inside the data (the module's own relocation records) filled at load time with PC addresses,
    stored big-endian like the rest of the data (port-gaps.md 6.3);
  - the setup stores `pcrel.py` derives from the module's prolog / epilog;
  - `pc_rel_link(module id)` / `pc_rel_unlink(module id)`: what the PC's Relocate / OSUnlink call for these modules
    (fix the module's pointers, perform the prolog stores; epilog stores on unlink). Returns 0 if the id is unknown.
Modules the build decompiles (config/GC6E01/<name>) are skipped: their C is compiled in.
*Changed 2026-11-01:* by default none of the above is written any more (pc_rel.c links every module from the loaded
bytes); the output keeps only the main.dol address table below and the debug scans. --prebuilt / ORRE_REL_PREBUILT=1
writes the module data again, for comparisons.
Usage (repo root): python -I tools/local/port/pcreldata.py [--out build-pc/generated] [--extracted DIR]
  (the extraction: --extracted, else $ORRE_EXTRACTED, else ./extracted, else ../pkmn_colosseum/extracted)
"""
import bisect
import collections
import os
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pcrel  # noqa: E402

MARKER = "ORREPORT-" + "GENERATED-FROM-DISC"


def main_symbol_table():
    """Sorted [(address, size, name)] of main-program symbols, for resolving addresses inside symbols."""
    out = []
    for line in Path("config/GC6E01/symbols.txt").read_text(encoding="utf-8").splitlines():
        m = re.match(r"(\S+) = \.\w+:0x([0-9A-Fa-f]+); // type:(\w+)(?: size:0x([0-9A-Fa-f]+))?", line)
        if m and re.fullmatch(r"[A-Za-z_]\w*", m.group(1)):
            out.append((int(m.group(2), 16), int(m.group(4) or "0", 16), m.group(1), m.group(3)))
    out.sort()
    return out


def resolve_main(table, addrs, a):
    i = bisect.bisect_right(addrs, a) - 1
    while i >= 0 and table[i][0] <= a:
        start, size, name, kind = table[i]
        if a == start or a < start + max(size, 1):
            return name, a - start, kind
        i -= 1
    return None


def built_names():
    return {p.name for p in Path("config/GC6E01").iterdir() if p.is_dir()}


def generate(out_dir, prebuilt=None):
    """prebuilt: also write the modules' data (the old pre-built linker). Default off (2026-11-01, the reference playthrough Part 1
    item 2c step 4): every module is linked by pc_rel.c from the bytes the game loads from the disc, so the exe carries
    none of their data; all 36 replay sittings end unchanged that way. ORRE_REL_PREBUILT=1 (or --prebuilt) brings the
    data back, for comparisons only. Without it the output keeps only the main.dol address table and the debug scans."""
    if prebuilt is None:
        prebuilt = os.environ.get("ORRE_REL_PREBUILT") == "1" or "--prebuilt" in sys.argv
    table = main_symbol_table()
    addrs = [t[0] for t in table]
    built = built_names()
    rels = {p.stem: p for p in sorted(pcrel.extracted_dir().rglob("*.rel"))}
    if not rels:
        # an error, not a warning: an empty resolver table still compiles and links, and the game then crashes at the
        # first module setup (2026-10-18: a worktree build without ORRE_EXTRACTED crashed in soundInit at frame 154)
        raise SystemExit(f"pcreldata: no .rel files under {pcrel.extracted_dir()} "
                         "(pass --extracted DIR or set ORRE_EXTRACTED)")
    lines, relocs, stores, unsure, externs, mods = [], [], [], [], {}, []
    be_ranges = []  # (module id, section, array, size): initialized data kept big-endian, for the ORRE_RELPTR_LOG scan
    main_refs = set()
    report = collections.Counter()

    def main_expr(a):
        r = resolve_main(table, addrs, a)
        if r is None:
            return None
        name, off, kind = r
        externs[name] = f"extern void {name}(void);" if kind == "function" else f"extern char {name}[];"
        return f"((char*)&{name} + {off})" if off else f"((char*)&{name})"

    for stem, p in sorted(rels.items()):
        name = stem.split("_", 1)[1] if stem[:3] in ("00_", "01_") else stem
        d = p.read_bytes()
        h, secs, rel = pcrel.parse_rel(d)
        # every main.dol address any module refers to, for pc_rel.c's resolver (modules linked from the disc)
        main_refs.update(a for (typ, tmod, tsec, a) in rel.values() if tmod == 0 and typ not in (0, 201, 202))
        if name in built:
            report["skipped: decompiled in the build"] += 1
            continue
        if not prebuilt:
            report["linked from the disc at run time (no data generated)"] += 1
            continue
        mid = h["id"]
        cname = re.sub(r"\W", "_", name)
        arrays = {}
        for i, s in enumerate(secs):
            if not s["size"] or s["code"]:
                continue
            arr = f"relmod_{cname}_s{i}"
            arrays[i] = arr
            if s["off"]:
                body = ", ".join(f"0x{x:02X}" for x in d[s["off"]:s["off"] + s["size"]])
                lines.append(f"static u8 {arr}[{s['size']}] __attribute__((aligned(32))) = {{{body}}};")
                unsure.append((f"{name} section {i}", f"0x{s['size']:X}", "room-module data, record types not settled: raw big-endian"))
                be_ranges.append((mid, i, arr, s["size"]))
            else:
                lines.append(f"static u8 {arr}[{s['size']}] __attribute__((aligned(32)));")
        if h["bssSize"] and not any(not s["off"] and s["size"] for s in secs):
            pass
        # pointer words inside the module's data (its own relocation records)
        for (sec, off), (typ, tmod, tsec, add) in sorted(rel.items(), key=lambda kv: (kv[0][0] or 0, kv[0][1])):
            if sec not in arrays:
                continue  # relocations inside code: handled by the prolog store list
            if typ != pcrel.R_ADDR32:
                unsure.append((f"{name} section {sec}+0x{off:X}", "4", f"relocation type {typ} in data"))
                report["data relocations: unsupported type"] += 1
                continue
            if tmod == mid and tsec in arrays:
                expr = f"({arrays[tsec]} + {add})"
            elif tmod == 0:
                expr = main_expr(add)
            else:
                expr = None
            if expr is None:
                unsure.append((f"{name} section {sec}+0x{off:X}", "4", f"pointer to module {tmod} / address 0x{add:X}: not resolvable"))
                report["data relocations: unresolved"] += 1
                continue
            relocs.append((mid, f"({arrays[sec]} + {off})", expr))
            report["data relocations: resolved"] += 1
        # setup stores from the prolog / epilog
        snames = pcrel.section_names(name, secs)
        res = {}
        for which in ("prolog", "epilog"):
            st, problem = pcrel.run(d, h, secs, rel, which, {}, snames)
            if problem:
                unsure.append((f"{name} {which}", "-", f"needs C: {problem}"))
                report[f"{which}: needs C"] += 1
                st = []
            res[which] = st
        for which in ("prolog", "epilog"):
            for slot, val in res[which]:
                # slot: ("main", "0xADDR"-or-name) - pcrel was given no names, so it is "0x%08X"
                sa = int(slot[1], 16) if slot[1].startswith("0x") else None
                sexpr = main_expr(sa) if slot[0] == "main" and sa is not None else None
                if val[0] == "addr" and val[1][0] == "module":
                    secname, voff = val[1][1].split("+0x")
                    sec_idx = next((i for i, n in snames.items() if n == secname), None)
                    vexpr = f"({arrays[sec_idx]} + {int(voff, 16)})" if sec_idx in arrays else None
                elif val[0] == "addr" and val[1][0] == "main":
                    vexpr = main_expr(int(val[1][1], 16)) if val[1][1].startswith("0x") else None
                elif val[0] == "const":
                    vexpr = f"((char*)0 + {val[1]})" if val[1] else "0"
                else:
                    vexpr = None
                if sexpr is None or vexpr is None:
                    unsure.append((f"{name} {which}", "-", f"store {slot} <- {val} not resolvable"))
                    report["setup stores: unresolved"] += 1
                    continue
                stores.append((mid, which, sexpr, vexpr))
                report["setup stores: resolved"] += 1
        mods.append((mid, name))
        report["modules generated"] += 1

    resolve = []
    for a in sorted(main_refs):
        e = main_expr(a)
        if e is None:
            report["main.dol addresses: not in symbols.txt"] += 1
            continue
        resolve.append((a, e))
    report["main.dol addresses for the resolver"] = len(resolve)
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    head = [f"/* {MARKER}: room modules the build doesn't decompile, generated by tools/local/port/pcreldata.py from the",
            " * player's disc. Never commit (the git hooks refuse it); regenerate instead. */",
            "#include <stdint.h>", "#include <stdio.h>", "#include <stdlib.h>", "#include <string.h>",
            "typedef uint8_t u8; typedef uint32_t u32;", ""]
    body = head + sorted(externs.values()) + [""] + lines + [""]
    body += ["struct pc_rel_ptr { u32 module; u8* slot; void* target; };",
             "struct pc_rel_ptr pc_rel_ptrs[] = {"]
    body += [f"    {{{m}, (u8*){s}, (void*){t}}}," for m, s, t in relocs] or ["    {0, 0, 0},"]
    body += ["};",
             "struct pc_rel_store { u32 module; int epilog; u32* slot; void* value; };",
             "struct pc_rel_store pc_rel_stores[] = {"]
    body += [f"    {{{m}, {1 if w == 'epilog' else 0}, (u32*){s}, (void*){v}}}," for m, w, s, v in stores] or ["    {0, 0, 0, 0},"]
    body += ["};",
             "const u32 pc_rel_ids[] = {" + (", ".join(str(m) for m, _ in mods) or "0") + "}; /* 0: none (no REL has id 0) */",
             "static u32 pc_rel_word(void* p)",
             "{",
             "    uintptr_t v = (uintptr_t)p;",
             "    if (v >> 32) { fprintf(stderr, \"pc_rel: address above 4 GB (%p)\\n\", p); abort(); }",
             "    return (u32)v;",
             "}",
             "static void pc_rel_put_be(u8* s, u32 w) { s[0] = (u8)(w >> 24); s[1] = (u8)(w >> 16); s[2] = (u8)(w >> 8); s[3] = (u8)w; }",
             "/* Returns 1 if the module is one of these generated modules (and links it), 0 otherwise. */",
             "int pc_rel_link(u32 module)",
             "{",
             "    int known = 0;",
             "    for (size_t i = 0; i < sizeof(pc_rel_ids) / sizeof(pc_rel_ids[0]); i++) if (module && pc_rel_ids[i] == module) known = 1;",
             "    if (!known) return 0;",
             "    for (size_t i = 0; i < sizeof(pc_rel_ptrs) / sizeof(pc_rel_ptrs[0]); i++)",
             "        if (pc_rel_ptrs[i].module == module && pc_rel_ptrs[i].slot) pc_rel_put_be(pc_rel_ptrs[i].slot, pc_rel_word(pc_rel_ptrs[i].target));",
             "    /* the prolog's stores go into main-program variables, which the game's C reads natively: host order */",
             "    for (size_t i = 0; i < sizeof(pc_rel_stores) / sizeof(pc_rel_stores[0]); i++)",
             "        if (pc_rel_stores[i].module == module && !pc_rel_stores[i].epilog && pc_rel_stores[i].slot)",
             "            *pc_rel_stores[i].slot = pc_rel_word(pc_rel_stores[i].value);",
             "    return 1;",
             "}",
             "int pc_rel_unlink(u32 module)",
             "{",
             "    int known = 0;",
             "    for (size_t i = 0; i < sizeof(pc_rel_stores) / sizeof(pc_rel_stores[0]); i++)",
             "        if (pc_rel_stores[i].module == module && pc_rel_stores[i].epilog && pc_rel_stores[i].slot) {",
             "            *pc_rel_stores[i].slot = pc_rel_word(pc_rel_stores[i].value); known = 1; }",
             "    return known;",
             "}",
             "/* main.dol address -> PC address, for pc_rel.c (pc_rel_set_resolver): the modules linked from the disc",
             " * (common_rel, the decompiled rooms) point into main.dol and their prologs store into its variables. */",
             "static const struct { u32 gc; char* pc; } pc_rel_main_table[] = {"]
    body += [f"    {{0x{a:08X}u, {e}}}," for a, e in resolve] or ["    {0, 0},"]
    body += ["};",
             "u32 pc_rel_resolve_main(u32 gc)",
             "{",
             "    size_t lo = 0, hi = sizeof(pc_rel_main_table) / sizeof(pc_rel_main_table[0]);",
             "    while (lo < hi) {",
             "        size_t mid = (lo + hi) / 2;",
             "        if (pc_rel_main_table[mid].gc < gc) lo = mid + 1; else hi = mid;",
             "    }",
             "    if (lo < sizeof(pc_rel_main_table) / sizeof(pc_rel_main_table[0]) && pc_rel_main_table[lo].gc == gc)",
             "        return pc_rel_word(pc_rel_main_table[lo].pc);",
             "    return 0;",
             "}",
             f"const unsigned pc_rel_store_count = {len(stores)}, pc_rel_ptr_count = {len(relocs)}, pc_rel_id_count = {len(mods)};",
             "/* ORRE_RELPTR_LOG=1 (2026-10-26; observes only; called at each room entry by pc_debug_game.c): every",
             " * main-program variable the modules refer to whose 32-bit value points into a module's initialized data, which",
             " * stays big-endian on the PC (the generated arrays below, and the modules pc_rel.c links at run time). The game's",
             " * C must read through such a pointer with the big-endian views (DISC_*): the shops' item lists were read natively",
             " * (P11, 2026-10-26). One line per variable and target section, the first time it is seen. */",
             "static const struct { u32 module; u32 section; const u8* start; u32 size; } pc_rel_be_ranges[] = {"]
    body += [f"    {{{m}, {i}, {a}, {z}}}," for m, i, a, z in be_ranges] or ["    {0, 0, 0, 0},"]
    body += ["};",
             "extern int pc_rel_linked_data_range(int index, u32* module, u32* section, u32* lo, u32* hi); /* pc_rel.c */",
             "void pc_rel_scan_main_pointers(unsigned room)",
             "{",
             "    static u32 seen_gc[2048], seen_target[2048];",
             "    static unsigned nseen;",
             "    for (size_t i = 0; i < sizeof(pc_rel_main_table) / sizeof(pc_rel_main_table[0]); i++) {",
             "        u32 v, mod = 0, sec = 0, off = 0, key;",
             "        int hit = 0;",
             "        unsigned k;",
             "        if (pc_rel_main_table[i].pc == 0) continue;",
             "        memcpy(&v, pc_rel_main_table[i].pc, 4);",
             "        if (v == 0) continue;",
             "        for (size_t r = 0; r < sizeof(pc_rel_be_ranges) / sizeof(pc_rel_be_ranges[0]) && !hit; r++) {",
             "            const u32 lo = (u32)(uintptr_t)pc_rel_be_ranges[r].start;",
             "            if (pc_rel_be_ranges[r].start && v >= lo && v < lo + pc_rel_be_ranges[r].size) {",
             "                hit = 1; mod = pc_rel_be_ranges[r].module; sec = pc_rel_be_ranges[r].section; off = v - lo;",
             "            }",
             "        }",
             "        for (int r = 0; !hit; r++) {",
             "            u32 m, s, lo, hi;",
             "            if (!pc_rel_linked_data_range(r, &m, &s, &lo, &hi)) break;",
             "            if (v >= lo && v < hi) { hit = 1; mod = m; sec = s; off = v - lo; }",
             "        }",
             "        if (!hit) continue;",
             "        key = mod << 8 | sec;",
             "        for (k = 0; k < nseen && !(seen_gc[k] == pc_rel_main_table[i].gc && seen_target[k] == key); k++) {}",
             "        if (k < nseen) continue;",
             "        if (nseen < sizeof(seen_gc) / sizeof(seen_gc[0])) { seen_gc[nseen] = pc_rel_main_table[i].gc; seen_target[nseen++] = key; }",
             "        fprintf(stderr, \"[relptr] room 0x%X: main 0x%08X -> module %u section %u +0x%X\\n\", room,",
             "                (unsigned)pc_rel_main_table[i].gc, (unsigned)mod, (unsigned)sec, (unsigned)off);",
             "    }",
             "}",
             "/* ORRE_RELPTR_LOG=2 (2026-10-26): also every 32-bit word of the exe's data and bss whose value points into",
             " * that big-endian module data, so copies of the pointers in other variables show up too (gs_floor.c keeps the",
             " * floor table in lbl_8047ACD0). PC addresses: map them to names with nm on the exe. */",
             "extern char __data_start__[], __bss_end__[];",
             "static int pc_rel_be_target(u32 v, u32* mod, u32* sec, u32* off)",
             "{",
             "    for (size_t r = 0; r < sizeof(pc_rel_be_ranges) / sizeof(pc_rel_be_ranges[0]); r++) {",
             "        const u32 lo = (u32)(uintptr_t)pc_rel_be_ranges[r].start;",
             "        if (pc_rel_be_ranges[r].start && v >= lo && v < lo + pc_rel_be_ranges[r].size) {",
             "            *mod = pc_rel_be_ranges[r].module; *sec = pc_rel_be_ranges[r].section; *off = v - lo; return 1;",
             "        }",
             "    }",
             "    for (int r = 0;; r++) {",
             "        u32 m, s, lo, hi;",
             "        if (!pc_rel_linked_data_range(r, &m, &s, &lo, &hi)) return 0;",
             "        if (v >= lo && v < hi) { *mod = m; *sec = s; *off = v - lo; return 1; }",
             "    }",
             "}",
             "void pc_rel_scan_globals(unsigned room)",
             "{",
             "    static u32 seen[8192];",
             "    static unsigned nseen;",
             "    const u8* p = (const u8*)(((uintptr_t)__data_start__ + 3) & ~(uintptr_t)3);",
             "    for (; p + 4 <= (const u8*)__bss_end__; p += 4) {",
             "        u32 v, mod, sec, off, at = (u32)(uintptr_t)p, dummy0, dummy1, dummy2;",
             "        unsigned k;",
             "        memcpy(&v, p, 4);",
             "        if (v < 0x10000 || !pc_rel_be_target(v, &mod, &sec, &off)) continue;",
             "        if (pc_rel_be_target(at, &dummy0, &dummy1, &dummy2)) continue; /* a word inside module data itself */",
             "        for (k = 0; k < nseen && seen[k] != at; k++) {}",
             "        if (k < nseen) continue;",
             "        if (nseen < sizeof(seen) / sizeof(seen[0])) seen[nseen++] = at;",
             "        fprintf(stderr, \"[relglob] room 0x%X: pc 0x%08X -> module %u section %u +0x%X\\n\", room, (unsigned)at,",
             "                (unsigned)mod, (unsigned)sec, (unsigned)off);",
             "    }",
             "}", ""]
    (out / "pc_rel_modules.c").write_text("\n".join(body), encoding="utf-8")
    (out / "pc_rel_unsure.txt").write_text(f"# {MARKER}: unsure room-module data\n" +
                                           "".join(f"{a}\t{b}\t{c}\n" for a, b, c in unsure), encoding="utf-8")
    for k, v in sorted(report.items()):
        print(f"{k}: {v}")
    print(f"-> {out / 'pc_rel_modules.c'}")
    return sorted(n for n, e in externs.items() if e.startswith("extern char"))  # main-program data the modules use


if __name__ == "__main__":
    generate(sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else "build-pc/generated")
