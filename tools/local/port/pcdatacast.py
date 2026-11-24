"""Make a file's conflicting declarations of one data label agree on the PC (OrrePort; compile-error tail).

MWCC accepts the same global declared with different types in different functions of one file
(`extern u8 lbl_802EF0A8[];` here, `extern MenuCBBattleStartSpriteEntry lbl_802EF0A8[];` there); GCC rejects it
("conflicting types"). This reads build-pc/pc_probe.log, and for each "conflicting types for 'lbl_...'" error at a
one-line `extern T name;` / `extern T name[];` declaration it rewrites that line, on the PC only, to the type of the
file's first declaration of the label, plus a cast macro that gives the code below the type it expects:

    #ifndef TARGET_PC
        extern MenuCBBattleStartSpriteEntry lbl_802EF0A8[];
    #else
        extern u8 lbl_802EF0A8[];                                          (the file's first declaration)
    #define lbl_802EF0A8 ((MenuCBBattleStartSpriteEntry*)lbl_802EF0A8)     (self-reference doesn't re-expand)
    #endif

Only block-scope declarations are changed (a file-scope macro would also rewrite later declarations); each gets `#ifdef TARGET_PC #undef ... #endif` after its function's closing brace. Scalars use
`(*(T*)&name)` so they stay assignable. A scalar pair where one side is a pointer and the other an integer is left
alone (different sizes on the 64-bit PC: a pointer-audit question). Only labels (`lbl_...`); files using __LINE__ are skipped. GameCube build
unchanged; verify with ninja, 26 SHA-1, fullsnap.

--pointer-int also unifies a pointer and a 32-bit integer view of one slot (decided 2026-10-15: pcdata.py reserves
8 aligned bytes for every 4-byte slot, so on the little-endian PC a u32 view reads the low half, which under the low-4 GB
rule is the whole pointer). A file-scope declaration is changed only when no later declaration of the label follows.

Usage (repo root): python -I tools/local/port/pcdatacast.py [--dry-run] [--pointer-int] [--log FILE]
"""
import re
import sys
from pathlib import Path

ERR = re.compile(r"^(?:\S+: )?\.?/?([\w/.-]+\.(?:c|cpp)):(\d+):\d+: error: conflicting types for '(lbl_\w+)'")
DECL = re.compile(r"^(\s*)extern\s+((?:const\s+)?[\w\s\*]+?)\s*\b(lbl_\w+)\s*(\[[^\]]*\])?\s*;\s*(/\*.*\*/)?\s*$")


def main():
    dry = "--dry-run" in sys.argv
    sites = {}
    log = sys.argv[sys.argv.index("--log") + 1] if "--log" in sys.argv else "build-pc/pc_probe.log"
    for line in Path(log).read_text(encoding="utf-8", errors="replace").splitlines():
        m = ERR.match(line)
        if m:
            sites.setdefault(m.group(1), {})[int(m.group(2))] = m.group(3)
    done = left = 0
    for f, lines in sorted(sites.items()):
        p = Path(f)
        if not p.exists():
            continue
        b = p.read_bytes().decode("utf-8", errors="surrogateescape")
        if "__LINE__" in b:
            print(f"SKIP {f}: uses __LINE__")
            continue
        nl = "\r\n" if "\r\n" in b else "\n"
        L = b.split(nl)
        for n in sorted(lines, reverse=True):
            name = lines[n]
            m = DECL.match(L[n - 1])
            first = next((DECL.match(x) for x in L if DECL.match(x) and DECL.match(x).group(3) == name), None)
            if not m or not first or first is m or L[n - 2].strip().startswith("#ifndef TARGET_PC /* PC: one type"):
                print(f"leave {f}:{n}: {L[n - 1].strip()[:90]}")
                left += 1
                continue
            ind, typ, arr = m.group(1), m.group(2).strip(), m.group(4)
            if not ind and any(DECL.match(x) and DECL.match(x).group(3) == name for x in L[n:]):
                # file scope with a later declaration of the label: the macro would rewrite that declaration too
                print(f"leave {f}:{n}: file-scope declaration followed by another declaration")
                left += 1
                continue
            ftyp, farr = first.group(2).strip(), first.group(4)
            if (typ, bool(arr)) == (ftyp, bool(farr)):
                print(f"leave {f}:{n}: same type as the first declaration")
                left += 1
                continue
            if not arr and not farr and ("*" in typ) != ("*" in ftyp) and "--pointer-int" not in sys.argv:
                # a pointer read through a 32-bit integer label (or the reverse) has a different size on the 64-bit PC:
                # a real pointer-width question (the project notes), not a declaration clash
                print(f"leave {f}:{n}: pointer vs integer ({typ} / {ftyp}): pointer audit")
                left += 1
                continue
            if arr and farr:
                cast = f"(({typ}*){name})"
            elif arr:
                cast = f"(({typ}*)&{name})"
            elif farr:
                cast = f"(*({typ}*){name})"
            else:
                cast = f"(*({typ}*)&{name})"
            new = ["#ifndef TARGET_PC /* PC: one type per label in a file (GCC); the code below sees its own type through a cast */",
                   L[n - 1], "#else", f"{ind}extern {ftyp} {name}{farr or ''};", f"#define {name} {cast}", "#endif"]
            # block scope: #undef after the enclosing function's closing brace
            undef_at = None
            if ind:
                depth = 0
                for k in range(n - 1, -1, -1):  # find the function's opening brace depth
                    depth += L[k].count("}") - L[k].count("{")
                    if depth < 0:
                        break
                depth = 0
                for k in range(n, len(L)):
                    depth += L[k].count("{") - L[k].count("}")
                    if depth < 0:
                        undef_at = k
                        break
            if not dry:
                if undef_at is not None:
                    L[undef_at + 1:undef_at + 1] = ["#ifdef TARGET_PC", f"#undef {name}", "#endif"]
                L[n - 1:n] = new
            done += 1
            print(f"cast {f}:{n}: {name} {typ}{arr or ''} over {ftyp}{farr or ''}")
        if not dry:
            p.write_bytes(nl.join(L).encode("utf-8", errors="surrogateescape"))
    print(f"{done} declarations unified on the PC, {left} left")
    return 0


if __name__ == "__main__":
    sys.exit(main())
