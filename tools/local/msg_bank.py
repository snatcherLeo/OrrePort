"""Print a message bank (resource type 5) unpacked by fsys_extract.py: header, then id and text per entry.
Usage: python -I msg_bank.py <bank file> [--codes]
Text is UTF-16BE; 0xFFFF + one byte is a control code, shown as <XX>, or <XX:hex> with its argument bytes (the code
table and argument sizes are in colo/msg.py CONTROL, from the game's msgctrlcode table). Prints to the terminal only:
never save the output in the repo (the project rules, extracted data). --codes prints control-code counts with handler names."""
import collections, os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from colo.msg import bank_text, CONTROL
sys.stdout.reconfigure(encoding="utf-8")
d = open(sys.argv[1], "rb").read()
bank, count = struct.unpack(">H", d[0:2])[0], struct.unpack(">H", d[4:6])[0]
lines = bank_text(d)
print(f"bank field {bank:#x}, {count} entries, strings from {0x10 + 8 * count:#x}")
if "--codes" in sys.argv:
    codes = collections.Counter(int(c, 16) for _, t in lines for c in re.findall(r"<([0-9A-F]{2})(?::[0-9A-F]*)?>", t))
    print("\n".join(f"{c:#04x} {CONTROL.get(c, ('(no handler)',))[0]}: {n}" for c, n in sorted(codes.items())))
else:
    print("\n".join(f"{k:#07x}  {t}" for k, t in lines))
