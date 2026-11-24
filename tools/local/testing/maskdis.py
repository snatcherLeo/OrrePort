"""Compare two PC exes' code with addresses, hex values and symbol names masked.

Used by the readability renames (docs/NAMING_SCHEME.md, "The readability renames"): names never reach compiled
code, so after a rename batch the new exe's .text must disassemble to the same instructions as the reference playthrough's exe once
addresses, constants and symbol names are masked. 0 differing lines = the code is the same.

Usage: python -I tools/local/testing/maskdis.py <objdump> <a.exe> <b.exe>
       (objdump: MSYS2's C:/msys64/mingw64/bin/objdump.exe)
Exit status 0 when identical, 1 otherwise.
"""
import re
import subprocess
import sys

HEX = re.compile(r"0x[0-9a-fA-F]+|\b[0-9a-fA-F]{6,16}\b")
SYM = re.compile(r"<[^>]*>")
LABEL = re.compile(r"^[0-9a-fA-F]+ <.*>:$")


def masked(objdump, exe):
    out = subprocess.run([objdump, "-d", "--no-show-raw-insn", "-j", ".text", exe],
                         capture_output=True, text=True, check=True).stdout
    lines = []
    for line in out.splitlines():
        if ":\t" not in line or LABEL.match(line.strip()):
            continue
        insn = line.split(":\t", 1)[1]
        insn = SYM.sub("<S>", insn)
        insn = HEX.sub("H", insn)
        lines.append(insn.strip())
    return lines


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    objdump, a, b = sys.argv[1:4]
    la, lb = masked(objdump, a), masked(objdump, b)
    diff = sum(1 for x, y in zip(la, lb) if x != y) + abs(len(la) - len(lb))
    print(f"instructions: {len(la)} vs {len(lb)}; differing lines: {diff}")
    return 0 if diff == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
