"""2026-11-10: draft names for the story flags, with the evidence, from the event-log replays (snatcherLeo: "For story
flags, we will name them by hand, but for now, keep what you think they should be called and why, so that'll help
later").

    python -I tools/local/port/flag_proposals.py <log> [<log> ...] --out flag_proposals.tsv

Give the logs in play order (the reference playthrough's P01 .. P16). For every flag the logs saw change, one row: the flag, a proposed
name, why (the rule that produced it), how it changed (one-time switch, counter, or a state that flips often), where
and when it first changed (sitting, frame, place), and the evidence: the event just before and just after that first
change (a battle won, a snag, an item received, a line of dialogue with its speaker).

The output holds the game's text (trainer names, dialogue), so it is written outside git (beside the recordings);
only this script is committed. The names are proposals for snatcherLeo to accept or change, never applied to the code here.
Rules, in order, for a flag's first change from 0:
  BEAT_<trainer>       a battle won (result 2) ended within 900 frames before, against the trainer whose
                       "would like to battle" line came last
  BATTLE_<trainer>_SEEN  the change comes with a battle's start (the same frame as "[battle] start")
  SNAGGED_<species>    a "Gotcha! <species> was caught!" line within 900 frames before
  GOT_<item>           an "obtained" / "received" line within 600 frames before
  TALKED_<speaker>     a dialogue line with a named speaker within 600 frames before
  EVENT_<place>        anything else: the place where it changed
A flag whose values go above 1 is a COUNTER; one that changes more than 6 times is a STATE (both keep the rule's name
as a hint).
"""
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import event_coverage as ec  # noqa: E402  (place_of, speaker_label, the line patterns)

FLAG = re.compile(r"^\[flag\] (-?\d+): (\d+) -> (\d+) \(frame (\d+), room 0x([0-9A-Fa-f]+)\)")
BTEXT = re.compile(r"^\[btext\] (.*?)\s{3}\(message 0x[0-9A-F]+, frame (\d+)\)")
START = re.compile(r"^\[battle\] start (\d+)")
RESULT = re.compile(r"^\[battle\] result (\d+)")
FRAME_HINT = re.compile(r"frame (\d+)")


def word(s):
    """a name part: letters and digits, words joined by underscores"""
    return "_".join(re.sub(r"[^A-Z0-9]+", " ", s.upper().replace("É", "E")).split())[:40]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    flags = {}       # id -> list of (sitting, frame, room, old, new)
    first = {}       # id -> (sitting, frame, room, before, after, rule, name)
    for log in a.logs:
        sitting = Path(log).parent.parent.name.split("_")[0]
        events = []  # (frame, kind, text) in log order
        frame = 0
        last_trainer = ""
        pending = []  # first changes waiting for their "after" event
        for line in Path(log).read_text(encoding="utf-8", errors="replace").splitlines():
            if m := FLAG.match(line):
                fid, old, new, fr, room = int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4)), m.group(5)
                frame = fr
                flags.setdefault(fid, []).append((sitting, fr, room, old, new))
                if fid not in first and old == 0:
                    rule, name, before = propose(events, fr, room, last_trainer)
                    first[fid] = [sitting, fr, ec.place_of(room), before, "", rule, name]
                    pending.append(fid)
                continue
            ev = None
            if m := START.match(line):
                ev = (frame, "start", f"battle {m.group(1)} starts")
            elif m := RESULT.match(line):
                ev = (frame, "result", m.group(1))
            elif m := BTEXT.match(line):
                frame = int(m.group(2))
                text = m.group(1)
                if "would like to battle" in text:
                    last_trainer = text.split(" would like")[0]
                ev = (frame, "btext", text)
            elif m := ec.DIALOG_LINE.match(line):
                text, key, box, fr, room, bank, speaker = m.groups()
                frame = int(fr)
                ev = (frame, "text", (ec.speaker_label(speaker, room, text) + ": " if speaker else "") + text)
            if ev:
                events.append(ev)
                for fid in pending:
                    first[fid][4] = f"{ev[1]}: {str(ev[2])[:90]}"
                pending = []

    with open(a.out, "w", encoding="utf-8", newline="") as f:
        f.write("flag\tproposed_name\trule\tkind\tchanges\tfirst_sitting\tfirst_frame\tplace\tbefore\tafter\n")
        for fid in sorted(flags):
            ch = flags[fid]
            kind = "COUNTER" if any(new > 1 for *_, new in ch) else ("STATE" if len(ch) > 6 else "SWITCH")
            if fid in first:
                s, fr, place, before, after, rule, name = first[fid]
            else:
                s, fr, place, before, after, rule, name = ch[0][0], ch[0][1], ec.place_of(ch[0][2]), "", "", "-", ""
            if not name:
                name = f"FLAG_{fid}"
            f.write("\t".join(str(x).replace("\t", " ") for x in
                              (fid, name, rule, kind, len(ch), s, fr, place, before, after)) + "\n")
    print(f"flags: {len(flags)}; with a first change from 0: {len(first)}; written: {a.out}")
    return 0


def propose(events, fr, room, last_trainer):
    """the rule, the name and the evidence for a flag's first change at frame fr"""
    place = ec.place_of(room)
    recent = [e for e in events if fr - e[0] <= 900]
    for e in reversed(recent):
        if e[1] == "start" and e[0] == fr:
            return "battle start", f"BATTLE_{word(last_trainer) or 'UNKNOWN'}_SEEN", e[2]
        if e[1] == "start" and fr - e[0] <= 5:
            return "battle start", f"BATTLE_{word(last_trainer) or 'UNKNOWN'}_SEEN", e[2]
    for e in reversed(recent):
        if e[1] == "result" and e[2] == "2":
            return "battle won", f"BEAT_{word(last_trainer) or 'UNKNOWN'}", f"won against {last_trainer}"
        if e[1] == "btext" and e[2].startswith("Gotcha!"):
            sp = e[2].replace("Gotcha! ", "").split(" was caught")[0]
            return "snag", f"SNAGGED_{word(sp)}", e[2]
    for e in reversed([e for e in recent if fr - e[0] <= 600]):
        if e[1] == "text" and re.search(r"\b(obtained|received)\b", e[2]):
            item = re.split(r"\b(?:obtained|received)\b", e[2])[-1].strip(" !.").split(" from ")[0]
            item = re.sub(r"^(a|an|the) ", "", item, flags=re.I)
            if "<name>" in item or not word(item):
                # the item's name is a placeholder the game fills in, not in the bank's text: name the place instead
                item = "ITEM_" + word(place.split(" (")[0])
            return "item", f"GOT_{word(item)}", e[2][:120]
    for e in reversed([e for e in recent if fr - e[0] <= 600]):
        if e[1] == "text" and ": " in e[2] and not e[2].startswith("NPC "):
            who = e[2].split(" (people")[0].split(":")[0]
            if who and who != "?" and len(who) < 25 and word(who):
                return "dialogue", f"TALKED_{word(who)}", e[2][:120]
    return "place", f"EVENT_{word(place.split(' (')[0])}_{room.upper()}", (recent[-1][2][:120] if recent else "")


if __name__ == "__main__":
    sys.exit(main())
