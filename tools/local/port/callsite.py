"""Show the original assembly leading up to one C call site (OrrePort; leftover-register calls, leftover-calls.md).

For `file:line` and a callee name: finds the C function around that line, its match score in the report, which
occurrence of the callee the line is inside that function, and prints the instructions before the matching `bl` in
the original assembly (build/GC6E01/asm), so the register values at the call can be read off. Read-only.

Usage (repo root): python -I tools/local/port/callsite.py src/game/x.c:123 CALLEE [--before N]
"""
import json
import re
import sys
from pathlib import Path

FN_START = re.compile(r"^(?!\s)(?!static\s+inline)[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\([^;]*$")


def enclosing(lines, n):
    """(name, start line) of the function definition enclosing 1-based line n (heuristic: last column-0 header)."""
    for i in range(n - 1, -1, -1):
        m = FN_START.match(re.sub(r"/\*.*?\*/|//.*$", "", lines[i].rstrip("\r\n")))
        if m and m.group(1) not in ("if", "while", "for", "switch", "return"):
            # a definition has `{` on this line or within the next 3 lines before any `;`
            tail = "".join(lines[i:i + 4])
            if "{" in tail and tail.find("{") < (tail.find(";") if ";" in tail else 1 << 30):
                return m.group(1), i + 1
    return None, None


def score(name):
    r = json.load(open("build/GC6E01/report.json", encoding="utf-8"))
    out = []
    for u in r["units"]:
        for f in u.get("functions", []):
            if f["name"] == name:
                out.append((u["name"], f.get("fuzzy_match_percent")))
    return out


def asm_body(name):
    for p in Path("build/GC6E01/asm").rglob("*.s"):
        t = p.read_text(encoding="utf-8", errors="replace")
        i = t.find(f".fn {name},")
        if i >= 0:
            j = t.find(".endfn", i)
            body = t[i:j].splitlines()
            return p, [re.sub(r"^/\* [0-9A-F]+ [0-9A-F]+  (?:[0-9A-F]{2} ){4}\*/\s*", "", l) for l in body]
    return None, []


def main():
    loc, callee = sys.argv[1], sys.argv[2]
    before = int(sys.argv[sys.argv.index("--before") + 1]) if "--before" in sys.argv else 14
    path, line = loc.rsplit(":", 1)
    line = int(line)
    lines = Path(path).read_text(encoding="utf-8", errors="replace").splitlines(keepends=True)
    name, start = enclosing(lines, line)
    if not name:
        print("no enclosing function found")
        return 1
    k = sum(len(re.findall(r"\b" + re.escape(callee) + r"\s*\(", lines[i])) for i in range(start, line - 1)
            if not lines[i].lstrip().startswith(("extern", "/*", "*", "//"))) \
        + (1 if re.search(r"\b" + re.escape(callee) + r"\s*\(", lines[line - 1]) else 0)
    print(f"{path}:{line} is in {name} (line {start}); call #{k} of {callee} there; report: {score(name) or 'not scored'}")
    ap, body = asm_body(name)
    if not body:
        print("no assembly found for", name)
        return 1
    hits = [i for i, l in enumerate(body) if re.search(r"\bbl\s+" + re.escape(callee) + r"\b", l)]
    print(f"{ap}: {len(hits)} `bl {callee}` in the function")
    if not hits:
        return 1
    i = hits[min(k, len(hits)) - 1]
    for l in body[max(0, i - before):i + 2]:
        print("   ", l)
    return 0


if __name__ == "__main__":
    sys.exit(main())
