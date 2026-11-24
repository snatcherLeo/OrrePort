#!/usr/bin/env python3
"""Check a translation of OrrePort's words against the English source (2026-11-13).

Why: snatcherLeo will ask people on Discord to translate OrrePort ("whatefver way we can make it, so that, when I gain
collaborators on discord to translate, it'll be very easy"). A translator runs this on their file and gets a plain
list of what to fix, without knowing the code. pc_text.h describes the files.

    python tools/local/lang_check.py de              # checks port/ui/lang/de.txt
    python tools/local/lang_check.py path/to/de.txt  # any file
    python tools/local/lang_check.py --all           # every language file in port/ui/lang/ and studio/lang/
    python tools/local/lang_check.py --studio de     # checks studio/lang/de.txt (against studio/lang/en.txt)

What it reports:
  - lines English has and the translation lacks (they will show in English: fine for a half-done file);
  - lines the translation has and English doesn't (a typo in a key, or a line OrrePort no longer uses);
  - a {name} missing, renamed or added (OrrePort fills only the names English has);
  - the count forms the language's "plural" rule needs (one / few / many / other) that are missing;
  - "<" or "&" in a text (the screens read them as layout);
  - texts much longer than English where English's note says "short" (they may be cut off: try "language = pseudo"
    and look).
Exit code 0 when nothing must be fixed (missing lines alone are fine), 1 otherwise.

Plain Python 3, no packages; the same on Windows, Linux and macOS.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LANG_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "port", "ui", "lang"))
STUDIO_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "studio", "lang"))
PLACEHOLDER = re.compile(r"\{([A-Za-z_][A-Za-z0-9_]*)\}")
FORMS = {
    "one-other": ["one", "other"],
    "french": ["one", "other"],
    "east-slavic": ["one", "few", "many"],
    "polish": ["one", "few", "many"],
    "czech": ["one", "few", "other"],
    "none": ["other"],
}


def read(path):
    """key -> (text, the note lines just above it)"""
    entries, note = {}, []
    with open(path, encoding="utf-8-sig") as f:
        for raw in f:
            line = raw.rstrip("\r\n")
            stripped = line.strip()
            if not stripped:
                note = []
                continue
            if stripped.startswith("#"):
                note.append(stripped[1:].strip())
                continue
            if "=" not in line:
                continue
            key, value = line.split("=", 1)
            entries[key.strip()] = (value.strip(), " ".join(note))
            note = []
    return entries


# lines whose {names} come in alternatives: a translation keeps one group or the other (2026-11-15: the SAVE FILES
# date is a 12-hour clock in English, {h} + {ampm}, and a 24-hour clock in most European languages, {hh}; OrrePort fills
# all three, en.txt's note says so)
ALTERNATIVES = {"game.save_files.date": [{"h", "ampm"}, {"hh"}]}
# lines that aren't text: settings of the file itself ("language.scope = game": only the in-game lines so far, so the
# hub doesn't offer the language yet; pc_text.cpp game_only)
META = ("plural", "language.name", "language.scope")


def check(path, en):
    code = os.path.splitext(os.path.basename(path))[0]
    tr = read(path)
    problems, notes = [], []
    rule = tr.get("plural", en.get("plural"))[0]
    if rule not in FORMS:
        problems.append('"plural = %s" is not one of: %s' % (rule, ", ".join(FORMS)))
        rule = "one-other"
    if "language.name" not in tr:
        notes.append('no "language.name" line: the language list would show it as "%s"' % code)

    # count keys: the base names English writes with .one / .other
    count_bases = {k.rsplit(".", 1)[0] for k in en if k.endswith(".other")}
    for base in sorted(count_bases):
        have = [f for f in ("one", "few", "many", "other") if base + "." + f in tr]
        if not have:
            continue  # not translated yet: English shows
        need = FORMS[rule]
        # every form the rule names, "other" too: with one form translated and another not, the count would show in
        # two languages depending on the number
        missing = [f for f in need if base + "." + f not in tr]
        if missing:
            problems.append("%s: your language's counts also need %s" % (base, ", ".join(base + "." + m for m in missing)))

    missing = [k for k in en if k not in tr and k.rsplit(".", 1)[0] not in count_bases]
    extra = [k for k in tr if k not in en and k not in META and not (k.rsplit(".", 1)[0] in count_bases and
                                                   k.rsplit(".", 1)[1] in ("one", "few", "many", "other"))]
    for k in extra:
        problems.append("%s: English has no such line (a typo in the key, or a line OrrePort no longer uses)" % k)

    for k, (text, _) in tr.items():
        base = k.rsplit(".", 1)[0]
        source = en.get(k) or en.get(base + ".other")
        if source is None:
            continue
        want = set(PLACEHOLDER.findall(source[0]))
        got = set(PLACEHOLDER.findall(text))
        if k in META:
            continue
        alts = ALTERNATIVES.get(k)
        if alts:
            used = next((g for g in alts if g <= got), None)
            if used is not None:  # one group is used whole: the other groups' names needn't be there
                want = (want - set().union(*alts)) | used
        if got - want:
            problems.append("%s: {%s} isn't filled by OrrePort (English has %s)" %
                            (k, "}, {".join(sorted(got - want)), ", ".join("{%s}" % w for w in sorted(want)) or "none"))
        if want - got:
            problems.append("%s: {%s} is missing (keep it, in any place in the sentence)" % (k, "}, {".join(sorted(want - got))))
        if "<" in text or "&" in text:
            problems.append("%s: please don't use < or & (the screens read them as layout)" % k)
        if "short" in source[1] and len(text) > max(len(source[0]) * 1.6, len(source[0]) + 8):
            notes.append("%s: %d letters where English has %d and the spot is short; check it on screen" %
                         (k, len(text), len(source[0])))

    done = len([k for k in en if k in tr])
    print("%s: %d of %d lines translated (%d%%)" % (path, done, len(en), done * 100 // max(1, len(en))))
    for p in problems:
        print("  FIX   " + p)
    for n in notes:
        print("  LOOK  " + n)
    if missing:
        print("  still in English (%d): %s%s" % (len(missing), ", ".join(missing[:12]), " ..." if len(missing) > 12 else ""))
    if not problems:
        print("  nothing to fix")
    return not problems


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    # *Added 2026-11-18:* Studio keeps its own words in studio/lang/ (decided 2026-11-18: its own program, so the
    # two sessions never edit one file). Each file is checked against the en.txt in its OWN folder, and --all covers
    # both folders; --studio <lang> picks Studio's file by language code.
    folders = [LANG_DIR, STUDIO_DIR]
    if argv[0] == "--all":
        paths = []
        for d in folders:
            if os.path.isdir(d):
                paths += [os.path.join(d, n) for n in sorted(os.listdir(d)) if n.endswith(".txt") and n != "en.txt"]
        if not paths:
            print("no translations yet (only en.txt)")
            return 0
    elif argv[0] == "--studio" and len(argv) > 1:
        paths = [os.path.join(STUDIO_DIR, argv[1] + ".txt")]
    else:
        a = argv[0]
        paths = [a if (os.sep in a or "/" in a or a.endswith(".txt")) else os.path.join(LANG_DIR, a + ".txt")]
    ok = True
    english = {}
    for p in paths:
        if not os.path.exists(p):
            print("%s: not found" % p)
            ok = False
            continue
        src = os.path.join(os.path.dirname(os.path.abspath(p)), "en.txt")
        if not os.path.exists(src):
            src = os.path.join(LANG_DIR, "en.txt")
        if src not in english:
            english[src] = read(src)
        ok = check(p, english[src]) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
