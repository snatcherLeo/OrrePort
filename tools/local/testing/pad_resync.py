"""Insert logged resync commands into a generated PC pad script (2026-10-27; the second pass's NPC resyncs).

    python -I tools/local/testing/pad_resync.py <run folder>/sitting.pad <spec file>

replay_check.py writes the pad script (dtm_to_pad.py); some timing splits need a resync the generator has no option
for, such as putting a wandering NPC (or Wes) where the console had it (`npc` debug command, port/src/pc_debug.c).
The spec says where, on the clock the presses already use: frames after a room visit's fade-in anchor
(`@0x3<room>`, dtm_to_pad.py --fadein), so the entry drift between the console and the PC cancels out.

Spec lines (`#` starts a comment):
    ROOM:VISIT +FRAMES COMMAND...      e.g.  0x3:2 +1610 npc 12 104.9602 0.0268 91.092 104.9602 0 91.092 4.786 1.316 4 0
The command goes in before the first step at or past that offset; that step's own wait shrinks by the same amount,
so every later press keeps its time. The file is rewritten in place (it is a scratch file of one run folder); run it
once per fresh `replay_check.py --prepare-only` folder, then run that folder (`replay_check.py <sitting> --inner
<folder> <sitting>` through saferun, then replay_report.py). Values come from Dolphin's npc probe (`playrec.py replay
N --probe npc`, PLAYREC_NPC); the match ledger logs each sitting's spec.
"""
import re
import sys


def step_lines(lines):
    """pad step number (1-based, as the PC's log counts them) -> line index: every non-comment line is one step"""
    out, n = {}, 0
    for i, line in enumerate(lines):
        if line.strip() and not line.startswith("#"):
            n += 1
            out[n] = i
    return out


def main():
    # pad_resync.py <pad> <spec> [--log <earlier run.log>]: "@F COMMAND" spec lines place COMMAND at PC frame F, using
    # the step frames that earlier run of the same settings logged ("[pad_script] frame X: step N ..."); for visits
    # whose presses hang on window anchors rather than a fade-in (the menu room's walks, P06)
    args = sys.argv[1:]
    log_path = None
    if "--log" in args:
        k = args.index("--log")
        log_path = args[k + 1]
        del args[k:k + 2]
    if len(args) != 2:
        raise SystemExit(__doc__)
    pad_path, spec_path = args
    lines = open(pad_path, encoding="utf-8").read().splitlines()
    specs, absolute = [], []
    for raw in open(spec_path, encoding="utf-8").read().splitlines():
        raw = raw.split("#")[0].strip()
        if not raw:
            continue
        where, *rest = raw.split()
        if where.startswith("@"):
            absolute.append((int(where[1:]), " ".join(rest)))
            continue
        offset, *cmd = rest
        room, visit = where.split(":")
        specs.append((int(room, 16), int(visit), int(offset.lstrip("+")), " ".join(cmd)))
    if absolute:
        if log_path is None:
            raise SystemExit("'@FRAME' spec lines need --log <an earlier run.log of the same settings>")
        fired = {}
        for line in open(log_path, encoding="utf-8", errors="replace"):
            m = re.match(r"\[pad_script\] frame (\d+): step (\d+) ", line)
            if m:
                fired.setdefault(int(m.group(2)), int(m.group(1)))
        where_line = step_lines(lines)
        # latest first, so earlier line numbers stay valid
        for frame, cmd in sorted(absolute, reverse=True):
            before = [(f, s) for s, f in fired.items() if f <= frame]
            if not before:
                raise SystemExit(f"@{frame}: no step fired before it in the log")
            f, s = max(before)
            i = where_line[s]
            gap = frame - f
            nxt = lines[i + 1] if i + 1 < len(lines) else ""
            m = re.match(r"\+(\d+):(.*)$", nxt)
            ins = [f"+{gap}:!{cmd}"]
            if m:  # the next relative step keeps its frame
                lines[i + 1] = f"+{max(0, int(m.group(1)) - gap)}:{m.group(2)}"
            lines[i + 1:i + 1] = ins
            print(f"@{frame}: after step {s} (frame {f}): !{cmd}")
    # latest insertions first, so earlier line numbers stay valid
    plans = []
    for room, visit, offset, cmd in specs:
        anchor = f"@0x3{room:04x}+"
        seen = 0
        start = None
        for i, line in enumerate(lines):
            if line.lower().startswith(anchor):
                seen += 1
                if seen == visit:
                    start = i
                    break
        if start is None:
            raise SystemExit(f"room 0x{room:x} visit {visit}: no fade-in anchor ({anchor}...) in the pad")
        cum = 0
        for j in range(start + 1, len(lines)):
            m = re.match(r"\+(\d+):(.*)$", lines[j])
            if m is None:  # the next anchor: the offset lies past this visit's relative steps (the room has ended)
                print(f"room 0x{room:x} visit {visit}: +{offset} is past the visit's steps (+{cum}): left out")
                break
            step = int(m.group(1))
            if cum + step >= offset:
                plans.append((j, cum, step, m.group(2), offset, cmd))
                break
            cum += step
    # several commands can fall before the same step: split its wait into the gaps between them
    by_step = {}
    for j, cum, step, rest, offset, cmd in plans:
        by_step.setdefault(j, (cum, step, rest, []))[3].append((offset, cmd))
    for j in sorted(by_step, reverse=True):
        cum, step, rest, cmds = by_step[j]
        new, at = [], cum
        for offset, cmd in sorted(cmds, key=lambda c: c[0]):
            new.append(f"+{offset - at}:!{cmd}")
            at = offset
            print(f"line {j + 1}: !{cmd}")
        new.append(f"+{cum + step - at}:{rest}")
        lines[j:j + 1] = new
    open(pad_path, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
