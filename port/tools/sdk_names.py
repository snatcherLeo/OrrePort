"""Real SDK names for the Dolphin SDK functions our game / HSD code calls by GameCube address (fn_800XXXXX), for the
PC link (docs/port/sdk-address-names.md).

The PC links against Aurora, which knows the SDK functions only by their real names, so every SDK function that our
code still calls by its address name needs a mapping. Evidence comes from Melee: same SDK library, linked in the same
library order, and the Melee decomp names it fully (config/GALE01/symbols.txt: name, address, size).

Method: both games' SDK functions in address order; functions named the same in both (anchors, e.g. __GXPEInit) pin
the two lists together; between two anchors the unnamed functions are paired with Melee's
  - by position when both stretches have the same number of functions, or
  - by a size alignment (longest common subsequence of sizes) when they do not.
Each pair is graded:
  same-size      both stretches have the same number of functions and this pair the same size (strong: likely)
  position       paired by position, sizes differ (an SDK version difference; weaker: check the caller)
  size-lcs       paired by the size alignment in a stretch of unequal length (weaker: check the caller)
  size-lcs-ambiguous
                 as size-lcs, but the size occurs more than once in the stretch (left out of the header)
  manual         named by hand: library, position in the SDK source order and the call pattern (MANUAL)
  rejected       a pairing checked against the callers and found wrong (REJECTED)
  after-anchor / before-anchor
                 the other anchor is missing or out of order (libraries linked in another order); counted from the
                 one anchor, same sizes only, stopping at the first size difference
Labels in the output stay "likely": a size match is not a byte match.

Usage (pc-platform repo root):
  python -I port/tools/sdk_names.py --melee ../melee [--all] [--csv out.csv] [--header out.h]
    default: only the SDK functions called by address name from src/game and src/hsd; --all: every unnamed SDK function
    --header: a candidate PC alias header (#define fn_800XXXXX RealName, with the evidence per line), like
    port/src/musyx_pc_alias.h; functions the port defines itself under the address name are left out (PORT_OWN)
"""
import argparse
import bisect
import collections
import csv
import glob
import re
import sys

# defined by the port under the address name on purpose (not aliased to Aurora's real name)
PORT_OWN = {
    0x800B90A4: "port/src/pc_gx_hooks.c: GX draw done delivered on the game thread",
    0x800A880C: "port/src/pc_vi.c (VISetPreRetraceCallback)",
    0x800A8850: "port/src/pc_vi.c (VISetPostRetraceCallback)",
    0x800B84E0: "GXSetArray: Aurora's PC version takes (attr, data, size, stride, le); the GameCube call has no size "
                "or byte order, so callers need a PC call site (docs/port/sdk-address-names.md, \"Wrong arity\")",
}

# pairings checked against the call sites and found wrong (2026-10-15): the stretch had as many functions as Melee's
# but different ones (another SDK version); the callers pass a struct + 0x18 like a condition variable
REJECTED = {
    0x8009F9C8: "sizes differ (0x20 vs 0x100); callers pass work + 0x18: guess OSInitCond",
    0x8009F9E8: "sizes differ (0xD4 vs 0x38); callers pass (p + 0x18, p): guess OSWaitCond",
    0x8009FABC: "sizes differ (0x20 vs 0x74); callers pass work + 0x18: guess OSSignalCond",
}

# named by hand (2026-10-15): library and position (SDK source order) plus the call pattern. Takes
# precedence over the automatic pairing. Label: likely, unless the note says guess.
MANUAL = {
    0x8009ED4C: ("OSLink", "OSLink.c; callers pass (module info, bss)"),
    0x8009EFE4: ("OSUnlink", "OSLink.c, after OSLink; callers pass the module info"),
    0x8009F1D0: ("OSInitMessageQueue", "OSMessage.c; (queue, message array, count)"),
    0x8009F230: ("OSSendMessage", "OSMessage.c, after OSInitMessageQueue; (queue, message, flags); the THP player's "
                                  "queues (THPPlayer.h), 2026-10-16"),
    0x8009F2F8: ("OSReceiveMessage", "OSMessage.c, after OSSendMessage; (queue, &message, flags)"),
    0x8009F3D4: ("OSGetPhysicalMemSize", "OSMemory.c; 0xC, no arguments; the memory report stores it as the total"),
    0x8009F488: ("OSProtectRange", "OSMemory.c; (channel, address, size, permission), e.g. (1, 0x81800000, 0x01800000, 3)"),
    0x8009F77C: ("OSInitMutex", "OSMutex.c, first function; callers pass the mutex"),
    0x8009F7B4: ("OSLockMutex", "OSMutex.c, after OSInitMutex; lock/unlock pairs around the same work"),
    0x8009F890: ("OSUnlockMutex", "OSMutex.c, before __OSUnlockAllMutex (named)"),
    0x8009F9C8: ("OSInitCond", "OSMutex.c after __OSUnlockAllMutex; 0x20 = a tail call (OSInitThreadQueue); callers pass work + 0x18"),
    0x8009F9E8: ("OSWaitCond", "OSMutex.c; (cond, mutex): callers pass (p + 0x18, p)"),
    0x8009FABC: ("OSSignalCond", "OSMutex.c; 0x20 = a tail call (OSWakeupThread); callers pass work + 0x18"),
    0x800BAE34: ("GXInitTexObjWrapMode", "verified from its decompiled body (sdk_range_800BA7C0.c): sets the wrap S / T bits "
                                  "of the texture object's mode word"),
    0x800A1E54: ("OSJoinThread", "OSThread.c; (thread, &result), main_tail joins the error thread with NULL"),
    0x800A257C: ("OSSetThreadPriority", "OSThread.c; (thread, priority), GBA_THREAD_PRIORITY"),
    0x800AAF38: ("PADInit", "PAD.c; no arguments, BOOL; caller comment PADInit"),
    0x800AB4FC: ("PADControlAllMotors", "PAD.c, after PADRead, before PADSetSpec (named); one array of u32 commands"),
    0x800B7874: ("GXSetVtxDesc", "GXAttr.c; (attr, type), e.g. (GX_VA_POS = 9, GX_DIRECT = 1)"),
    0x800B7D3C: ("GXClearVtxDesc", "GXAttr.c; no arguments, before GXSetVtxAttrFmt"),
    0x800B7D74: ("GXSetVtxAttrFmt", "GXAttr.c; (vtxfmt, attr, count, type, frac)"),
    0x800B8C58: ("GXSetDrawSync", "GXMisc.c, between GXAbortFrame and GXSetDrawDone (named); one token argument"),
    0x800B8FD8: ("GXSetDrawSyncCallback", "GXMisc.c, before GXTokenInterruptHandler (fn_800B901C); 0x44 like "
                                   "GXSetDrawDoneCallback; GSgapp's callback (fn_80101B34) takes a u16 token and checks "
                                   "token 3, which GSgapp sets with fn_800B8C58(3) (not GXSetBreakPtCallback)"),
    0x800B959C: ("GXSetDispCopySrc", "GXFrameBuf.c; (left, top, width, height) of the EFB"),
    0x800B962C: ("GXSetTexCopySrc", "GXFrameBuf.c, after GXSetDispCopySrc; (left, top, width, height)"),
    0x800B96BC: ("GXSetDispCopyDst", "GXFrameBuf.c; (fb width, xfb lines)"),
    0x800B96F8: ("GXSetTexCopyDst", "GXFrameBuf.c; (width, height, format, mipmap)"),
    0x800B9874: ("GXSetCopyClamp", "GXFrameBuf.c; GX_CLAMP_TOP | GX_CLAMP_BOTTOM"),
    0x800B9B14: ("GXSetDispCopyYScale", "GXFrameBuf.c; f32 scale -> u32 xfb lines"),
    0x800BB97C: ("GXSetIndTexCoordScale", "GXBump.c order; (stage, scale s, scale t)"),
    0x800BBAF8: ("GXSetIndTexOrder", "GXBump.c order; (stage, coord, map)"),
    0x800BBC0C: ("GXSetNumIndStages", "GXBump.c order; (count)"),
    0x800BBC34: ("GXSetTevDirect", "GXBump.c order, after GXSetNumIndStages; (stage); 0x48 as in Melee"),
    0x800BBC7C: ("GXSetTevIndWarp", "GXBump.c order; (stage, ind stage, signed, replace, mtx)"),
    0x800BBCE0: ("GXSetTevIndTile", "GXBump.c order; (stage, ind stage, tile sizes, spacings, ...)"),
    0x800BBE8C: ("GXSetTevIndBumpST", "GXBump.c order; (stage, ind stage, mtx)"),
    0x800BBF98: ("GXSetTevIndBumpXYZ", "GXBump.c order, after BumpST; (stage, ind stage, mtx)"),
    0x800BBFDC: ("GXSetTevIndRepeat", "GXBump.c order, after BumpXYZ; (stage); the size alignment's GXSetTevDirect was the other 0x48 function"),
    0x800BC618: ("GXSetAlphaCompare", "GXTev.c; (comp0, ref0, op, comp1, ref1)"),
    0x800BD2E0: ("GXSetProjection", "GXTransform.c; (Mtx44, projection type)"),
    0x800BD454: ("GXGetProjectionv", "GXTransform.c; fills an f32 array"),
    0x800BD91C: ("GXSetGPMetric", "GXPerf.c, after __GXSetMatrixIndex as in Melee (0x848 vs 0x898); (perf0, perf1)"),
    0x800BE30C: ("GXClearGPMetric", "GXPerf.c; 0x10 as in Melee; no arguments"),
    0x800BE31C: ("GXReadGP0Metric", "GXPerf.c after GXClearGPMetric; returns a u32 counter, read by GSgapp's draw-sync callback "
                             "at token 3 (also docs/port/aurora-coverage.md)"),
    0x800B5BE4: ("CARDSetAttributesAsync", "CARDStat.c, after CARDGetAttributes (named); (channel, ...) passed to the "
                                    "async result handler (also docs/port/aurora-coverage.md)"),
    0x800B01AC: ("__CARDGetDiskID", "verified from its body (sdk_exact_800B01AC.c): returns the channel's disk ID; before "
                             "__CARDGetControlBlock as in the SDK's CARDBios"),
    0x800B01C4: ("CARDSetDiskID", "verified from its body: sets the channel's disk ID (NULL = 0x80000000), returns BUSY / "
                           "READY; Aurora's card.h declares it with this signature"),
    0x800D104C: ("SIRefreshSamplingRate", "verified from its body: SISetSamplingRate(SamplingRate); 0x24 as in Melee"),
    0x800D0F44: ("SIProbe", "SI unit; (channel) -> device type, compared with the GBA type 0x40000; 0x24 = a wrapper of SIGetType"),
}

SYM = re.compile(r"^(\S+) = \.text:0x([0-9A-F]+); // type:function size:0x([0-9A-F]+)")


def symbols(path):
    out = []
    for line in open(path, encoding="utf-8"):
        m = SYM.match(line)
        if m:
            out.append((int(m.group(2), 16), m.group(1), int(m.group(3), 16)))
    return sorted(out)


def unit_ranges(splits):
    ranges, cur = [], None
    for line in open(splits, encoding="utf-8"):
        line = line.rstrip("\n")
        if line and line[0] not in " \t" and line.endswith(":"):
            cur = line[:-1]
        m = re.search(r"\.text\s+start:0x([0-9A-F]+)\s+end:0x([0-9A-F]+)", line)
        if m and cur:
            ranges.append((int(m.group(1), 16), int(m.group(2), 16), cur))
    return sorted(ranges)


def unnamed(name):
    return re.match(r"^(fn|lbl|func|zz)_[0-9A-Fa-f]{6,8}", name) is not None


def lcs_pairs(a, b):
    """Pairs (i, j) of a longest common subsequence of the size lists a and b"""
    n, m = len(a), len(b)
    t = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n - 1, -1, -1):
        for j in range(m - 1, -1, -1):
            t[i][j] = t[i + 1][j + 1] + 1 if a[i] == b[j] else max(t[i + 1][j], t[i][j + 1])
    i = j = 0
    pairs = []
    while i < n and j < m:
        if a[i] == b[j]:
            pairs.append((i, j))
            i += 1
            j += 1
        elif t[i + 1][j] >= t[i][j + 1]:
            i += 1
        else:
            j += 1
    return pairs


def align(ours, melee):
    """ours / melee: [(addr, name, size)] of SDK functions in address order -> {our addr: (melee name, grade, prev, next)}

    Anchors are our functions whose name Melee has. Each stretch between two consecutive anchors is aligned with
    Melee's functions between the same two names when those appear in the same order and close together (the two
    games link some libraries in a different order, so this is checked per stretch); otherwise the stretch is
    aligned one-sided from the anchor before (or after) it, and only same-size pairs are kept."""
    mel_index = {name: k for k, (a, name, s) in enumerate(melee) if not unnamed(name)}
    anchors = [i for i, (a, name, s) in enumerate(ours) if name in mel_index]
    result = {}

    def put(x, y, grade, i0, i1):
        if unnamed(x[1]):
            result[x[0]] = (y[1], grade, ours[i0][1] if i0 >= 0 else "-", ours[i1][1] if i1 < len(ours) else "-")

    bounds = [-1] + anchors + [len(ours)]
    for i0, i1 in zip(bounds, bounds[1:]):
        a = ours[i0 + 1:i1]
        if not any(unnamed(x[1]) for x in a):
            continue
        k0 = mel_index[ours[i0][1]] if i0 >= 0 else None
        k1 = mel_index[ours[i1][1]] if i1 < len(ours) else None
        if k0 is not None and k1 is not None and k0 < k1 <= k0 + 3 * len(a) + 8:
            b = melee[k0 + 1:k1]
            if len(a) == len(b):
                for x, y in zip(a, b):
                    put(x, y, "same-size" if x[2] == y[2] else "position", i0, i1)
            else:
                sizes = collections.Counter(x[2] for x in a) + collections.Counter(y[2] for y in b)
                for p, q in lcs_pairs([x[2] for x in a], [y[2] for y in b]):
                    # a size that occurs more than once in the stretch can pair the wrong one of two functions
                    put(a[p], b[q], "size-lcs" if sizes[a[p][2]] == 2 else "size-lcs-ambiguous", i0, i1)
            continue
        if k0 is not None:  # one-sided, forward from the anchor before
            for x, y in zip(a, melee[k0 + 1:k0 + 1 + len(a)]):
                if x[2] != y[2]:
                    break
                put(x, y, "after-anchor", i0, i1)
        if k1 is not None:  # one-sided, backward from the anchor after
            for x, y in zip(reversed(a), reversed(melee[max(0, k1 - len(a)):k1])):
                if x[2] != y[2]:
                    break
                if x[0] not in result:
                    put(x, y, "before-anchor", i0, i1)
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--melee", required=True)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--csv")
    ap.add_argument("--header")
    ap.add_argument("--defined", action="append", default=[],
                    help="file of symbol names the PC build defines (e.g. nm -g --defined-only of Aurora's and the "
                         "port's libraries, one name per line); adds whether each real name is provided")
    args = ap.parse_args()

    ranges = unit_ranges("config/GC6E01/splits.txt")
    starts = [r[0] for r in ranges]

    def unit(addr):
        i = bisect.bisect_right(starts, addr) - 1
        return ranges[i][2] if i >= 0 and addr < ranges[i][1] else None

    ours_all = symbols("config/GC6E01/symbols.txt")
    ours = [x for x in ours_all if (unit(x[0]) or "").startswith("dolphin/")]
    mel_all = symbols(f"{args.melee}/config/GALE01/symbols.txt")
    mel_ranges = unit_ranges(f"{args.melee}/config/GALE01/splits.txt")
    mel_starts = [r[0] for r in mel_ranges]

    def mel_unit(addr):
        i = bisect.bisect_right(mel_starts, addr) - 1
        return mel_ranges[i][2] if i >= 0 and addr < mel_ranges[i][1] else ""

    melee = [x for x in mel_all if mel_unit(x[0]).startswith(("dolphin/", "Runtime", "MSL", "MetroTRK"))]
    mapping = align(ours, melee)

    # a pair must stay inside one SDK library: Melee's unit path names it (dolphin/gx/...); ours does when the unit sits
    # in a library folder (dolphin/gx/GXTev.c), and generic units (dolphin/sdk_range_*.c) take the anchors' library
    def lib(path):
        parts = (path or "").split("/")
        return parts[1] if len(parts) > 2 and parts[0] == "dolphin" else None
    mel_lib = {name: lib(mel_unit(a)) or mel_unit(a).split("/")[0] for a, name, size in melee}
    ours_lib = {a: lib(unit(a)) for a, name, size in ours}
    for addr in list(mapping):
        name, grade, prev, nxt = mapping[addr]
        want = ours_lib.get(addr) or mel_lib.get(prev) or mel_lib.get(nxt)
        if want is not None and mel_lib.get(name) != want:
            del mapping[addr]

    called = collections.Counter()
    for f in glob.glob("src/game/**/*.c", recursive=True) + glob.glob("src/hsd/**/*.c", recursive=True):
        for m in re.finditer(r"\bfn_(80[0-9A-F]{6})\s*\(", open(f, encoding="utf-8", errors="replace").read()):
            called[int(m.group(1), 16)] += 1

    # address names our own SDK C defines (e.g. the THP player): the PC links them as they are
    own_c = {}
    for f in glob.glob("src/dolphin/**/*.c", recursive=True):
        for m in re.finditer(r"^[A-Za-z_][\w \*]*\b(fn_80[0-9A-F]{6})\s*\([^;{]*\)\s*\{", open(f, encoding="utf-8",
                             errors="replace").read(), re.M):
            own_c.setdefault(m.group(1), f.replace("\\", "/"))

    defined = set()
    for path in args.defined:
        defined |= {line.strip() for line in open(path, encoding="utf-8") if line.strip()}

    rows = []
    for addr, name, size in ours:
        if not unnamed(name) or (not args.all and addr not in called):
            continue
        mel = mapping.get(addr) if addr not in REJECTED else None
        if addr in MANUAL:
            mel = (MANUAL[addr][0], "manual", MANUAL[addr][1], "")
        rows.append(dict(addr=f"0x{addr:08X}", ours=name, size=f"0x{size:X}", unit=unit(addr), sites=called[addr],
                         sdk=mel[0] if mel else "",
                         grade=mel[1] if mel else ("rejected" if addr in REJECTED else "unresolved"),
                         between=(mel[2] if mel[1] == "manual" else f"{mel[2]} .. {mel[3]}") if mel else "",
                         provided=("yes" if mel and mel[0] in defined else "no") if defined and mel else ""))
    grades = collections.Counter(r["grade"] for r in rows)
    print(f"{len(rows)} functions; " + ", ".join(f"{g} {n}" for g, n in grades.most_common()))
    if defined:
        named = [r for r in rows if r["sdk"]]
        print(f"of the {len(named)} named: {sum(r['provided'] == 'yes' for r in named)} defined by the PC build, "
              f"{sum(r['provided'] == 'no' for r in named)} not")
    for r in rows:
        print(f"{r['addr']} {r['ours']:12s} {r['size']:>6s} {r['sites']:4d} sites  {r['sdk'] or '?':34s} {r['grade']:10s}"
              f"  {r['unit']}  [{r['between']}]")
    if args.header:
        head = [
            "/*",
            " * sdk_pc_alias.h: CANDIDATE, generated by port/tools/sdk_names.py (docs/port/sdk-address-names.md).",
            " * The SDK functions our game / HSD code calls by GameCube address, mapped to the real SDK names Aurora",
            " * defines. Evidence per line: the grade and the two named neighbours (Melee's symbol list, same SDK",
            " * library). PC-only, force-included into game / HSD units for the PC link; not a rename.",
            " */",
            "#ifndef SDK_PC_ALIAS_H",
            "#define SDK_PC_ALIAS_H",
            "",
        ]
        body = []
        for r in rows:
            a = int(r["addr"], 16)
            if a in PORT_OWN:
                body.append(f"/* {r['ours']}: kept, {PORT_OWN[a]} */")
            elif r["sdk"] and r["grade"] == "size-lcs-ambiguous":
                body.append(f"/* {r['ours']}: maybe {r['sdk']} (size alignment, ambiguous: check the callers) */")
            elif r["sdk"]:
                missing = ", NOT defined by the PC build yet" if r.get("provided") == "no" else ""
                body.append(f"#define {r['ours']} {r['sdk']} /* {r['grade']}, {r['size']}, [{r['between']}]{missing} */")
            elif not r["sdk"] and r["ours"] in own_c:
                body.append(f"/* {r['ours']}: kept, defined by our own C ({own_c[r['ours']]}) */")
            elif int(r["addr"], 16) in REJECTED:
                body.append(f"/* {r['ours']}: rejected match, {REJECTED[int(r['addr'], 16)]} ({r['unit']}) */")
            else:
                body.append(f"/* {r['ours']}: unresolved ({r['unit']}) */")
        with open(args.header, "w", encoding="utf-8", newline="") as f:
            f.write(chr(10).join(head + body + ["", "#endif", ""]))
    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
    return 0


if __name__ == "__main__":
    sys.exit(main())
