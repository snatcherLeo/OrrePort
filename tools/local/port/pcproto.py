"""Give a function one canonical prototype on the PC (OrrePort stage 2).

Game files declare central functions locally with guessed prototypes (`extern void* pokemonGetStatus();`,
`extern u8 pokemonGetStatus(u32, u32, u32, u32);` ...). MWCC accepts the mix; GCC rejects differing declarations in
one file, and on the PC a wrong prototype is undefined behaviour. For each named function this tool:
  1. takes the prototype from the function's live definition (the one in a unit the report scores, prototyped style);
  2. adds it to include/pc/prototypes.h (the PC build force-includes it: -include pc/prototypes.h);
  3. wraps every other declaration of the function in src/ and include/ in `#ifndef TARGET_PC ... #endif`.
The GameCube build's preprocessed source is unchanged (verify: full ninja + 26 SHA-1). Only functions whose
definition uses basic types (u8..f64, s8..s64, BOOL, void, and pointers to those) are handled; others are listed.

Usage: python -I tools/local/port/pcproto.py NAME [NAME ...]   (or --top N to take the N most conflicting names from
       build-pc/pc_probe.log; --rehide re-runs the hiding pass for every name already in prototypes.h)
Alias macros (`#define fn_8012640C pokemonGetStatus` ... `#undef`) are followed: their declarations are hidden too.
"""
import collections
import re
import sys
from pathlib import Path

CR, LF = chr(13), chr(10)
BASIC = r"(?:const\s+)?(?:unsigned\s+|signed\s+)?(?:u8|u16|u32|u64|s8|s16|s32|s64|f32|f64|BOOL|void|int|char|short|long|float|double)"
PARAM = re.compile(r"^\s*(?:" + BASIC + r"|(?:const\s+)?struct\s+\w+\s*\*)\s*(?:\*\s*(?:const\s*)?)*\s*\w*\s*$")
HDR = Path("include/pc/prototypes.h")

args = [a for i, a in enumerate(sys.argv[1:], 1) if not a.startswith("--") and sys.argv[i - 1] not in ("--exclude", "--top")]
if "--top" in sys.argv:
    n = int(sys.argv[sys.argv.index("--top") + 1])
    c = collections.Counter(re.findall(r"conflicting types for '(\w+)'", Path("build-pc/pc_probe.log").read_text(errors="replace")))
    args = [k for k, _ in c.most_common(n)]

files = [p for p in list(Path("src").rglob("*.c")) + list(Path("include").rglob("*.h"))
         if not p.as_posix().startswith(("src/dolphin", "src/trk", "src/crt", "include/pc/"))]
texts = {p: p.read_bytes().decode("utf-8", errors="surrogateescape") for p in files}
# --exclude PREFIX (repeatable): files another session owns right now; their declarations are not hidden
EXCLUDE = tuple(sys.argv[i + 1] for i, x in enumerate(sys.argv) if x == "--exclude")


def live_lines():
    """(file, line) pairs the GameCube build really compiles: every unit of `ninja -t commands all_source` run through
    `gcc -E` with its own -D defines (no TARGET_PC), so `#if 0` copies, inactive `#if !defined(...)` branches and files
    no unit includes are left out. Rebuilt on every run (about 5 s): hiding declarations shifts line numbers."""
    import concurrent.futures as cf
    import subprocess
    cmds = subprocess.run(["ninja", "-t", "commands", "all_source"], capture_output=True, text=True).stdout.splitlines()
    units = []
    for c in cmds:
        m = re.search(r" -c (\S+) -o \S+", c)
        if "mwcceppc.exe" in c and m and m.group(1).replace("\\", "/").startswith("src/"):
            units.append((m.group(1).replace("\\", "/"), re.findall(r"(?<=\s)-D(\S+)", c)))

    def run(u):
        src, defs = u
        r = subprocess.run(["gcc", "-E", "-x", "c", "-D__MWERKS__=0x4199", "-Iinclude", "-Ibuild/GC6E01/include", "-I."]
                           + [f"-D{d}" for d in defs] + [src], capture_output=True, text=True, errors="replace")
        out, cur, n = set(), None, 0
        for line in r.stdout.splitlines():
            m = re.match(r'^# (\d+) "([^"]+)"', line)
            if m:
                n, cur = int(m.group(1)), m.group(2).replace("\\\\", "/").replace("\\", "/").lstrip("./")
                continue
            if line[:1].strip() and "(" in line:  # column-0 text with a parenthesis: candidate definition lines
                out.add((cur, n))
            n += 1
        return out

    lines = set()
    with cf.ThreadPoolExecutor(16) as ex:
        for s in ex.map(run, units):
            lines |= s
    return lines


LIVE = None


def is_live(p, t, pos):
    global LIVE
    if LIVE is None:
        LIVE = live_lines()
    return (p.as_posix(), t.count(LF, 0, pos) + 1) in LIVE


def tagged(decl, t):
    """`Window* w` -> `struct Window* w` when the defining file t has `typedef struct Window {...} Window;` (or
    `typedef struct Tag Window;`): a prototype can name the struct by its tag without its full definition, and
    callers' void* converts implicitly. Leaves other types alone."""
    m = re.match(r"^(const\s+)?([A-Za-z_]\w*)\*(.*)$", decl.strip())
    if not m or re.fullmatch(BASIC, m.group(2)):
        return decl
    ty = m.group(2)
    g = re.search(r"typedef\s+struct\s+(\w+)\s*\{(?:[^{}]|\{[^{}]*\})*\}\s*" + re.escape(ty) + r"\s*;", t) or \
        re.search(r"typedef\s+struct\s+(\w+)\s+" + re.escape(ty) + r"\s*;", t)
    return f"{m.group(1) or ''}struct {g.group(1)}*{m.group(3)}" if g else decl


def definition(name):
    """Prototyped definitions of name: (return type, params) candidates."""
    found = collections.Counter()
    pat = re.compile(r"^(?!\s)((?:static\s+)?[\w\s\*]+?)\b" + re.escape(name) + r"\s*\(([^;{}()]*)\)\s*(?:\{.*)?$", re.M)  # body may start on the same line
    # also a definition with the return type on its own line: `u32` then `name(params)`
    pat2 = re.compile(r"^(?!\s)((?:static\s+)?[\w\s\*]+?)[ \t]*\r?\n" + re.escape(name) +
                      r"\s*\(([^;{}()]*)\)\s*(?:\{.*)?$", re.M)
    for p, t in texts.items():
        if not p.suffix == ".c":
            continue
        for m in list(pat.finditer(t)) + list(pat2.finditer(t)):
            ret, params = m.group(1).strip(), " ".join(m.group(2).split())
            ret, params = (re.sub(r"\s*\*\s*", "* ", x).replace("* *", "**").strip() for x in (ret, params))  # `void *p` == `void* p`
            params = params.replace("* ,", "*,")
            ret = tagged(ret + "*", t)[:-1] if ret.endswith("*") else ret
            if params and params not in ("void",):
                params = ", ".join(tagged(x, t) for x in params.split(", "))
            if "static" in ret or ret.startswith(("return", "else", "asm")) or not ret:
                continue
            if not is_live(p, t, m.start()):
                continue  # a dead copy: #if 0, an inactive branch, or a file no unit compiles
            nxt = t[m.end():m.end() + 200].lstrip()
            if not ("{" in m.group(0) or nxt.startswith("{")):
                continue
            if params and params != "void" and all(re.match(r"^\w+$", x.strip()) for x in params.split(",")):
                continue  # K&R
            found[(ret, params)] += 1
    # definitions that differ only in parameter names are the same function signature: keep the first spelling
    by_types = {}
    for (ret, params), k in found.items():
        types = tuple(re.sub(r"\s*\b\w+\s*$", "", x.strip()) if re.search(r"[\w\*]\s+\w+\s*$", x.strip()) else x.strip()
                      for x in params.split(","))
        by_types.setdefault((ret, types), ((ret, params), 0))
        key, n = by_types[(ret, types)]
        by_types[(ret, types)] = (key, n + k)
    return collections.Counter({key: n for key, n in by_types.values()})


decl_pat_cache = {}


def write_retry(p, data):
    """Windows sometimes refuses a write for a moment (errno 22 / 13 while another process - a build, an editor, the
    virus scanner - has the file open). Retry for a few seconds before giving up."""
    import time
    for attempt in range(20):
        try:
            p.write_bytes(data)
            return
        except OSError:
            if attempt == 19:
                raise
            time.sleep(0.25)


def hide(name):
    """Wrap every declaration of name in src/ and include/ in #ifndef TARGET_PC, including declarations made through
    an alias macro (`#define fn_8012640C pokemonGetStatus` ... `#undef fn_8012640C`) while that alias is active."""
    n = 0
    for p in files:
        t = texts[p]
        if name not in t or (EXCLUDE and p.as_posix().startswith(EXCLUDE)):
            continue
        nl = CR + LF if (CR + LF) in t else LF
        L = t.split(nl)
        out = []
        active = set()  # alias macros currently defined as name
        skip_to = -1
        for k, line in enumerate(L):
            if k <= skip_to:
                continue
            m = re.match(r"^\s*#\s*define\s+(\w+)\s+" + re.escape(name) + r"\s*$", line)
            if m:
                active.add(m.group(1))
            m = re.match(r"^\s*#\s*undef\s+(\w+)", line)
            if m:
                active.discard(m.group(1))
            names = [name] + sorted(active)
            core = re.sub(r"^\s*/\*.*?\*/\s*", "", line)  # a declaration may follow a short leading comment
            if any(decl_pat(x).match(core) and not call_args(core, x) for x in names if x in core) and \
                    not core.lstrip().startswith(("return", "#", "//", "/*", "*")) and \
                    not (k > 0 and L[k - 1].strip().startswith("#ifndef TARGET_PC")):
                out += ["#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */", line, "#endif"]
                n += 1
            elif any(x in line for x in names) and "(" in line and ";" not in line and "{" not in line and                     not line.lstrip().startswith(("return", "#", "//", "/*", "*", "if", "while", "for")) and                     not (k > 0 and L[k - 1].strip().startswith("#ifndef TARGET_PC")):
                # a declaration split over several lines: join up to the `;` (at most 6 lines) and test the whole
                e = next((j for j in range(k, min(k + 6, len(L))) if ";" in L[j] or "{" in L[j]), -1)
                joined = " ".join(x.strip() for x in L[k:e + 1]) if e >= 0 else ""
                if e > k and "{" not in joined and any(decl_pat(x).match(joined) and not call_args(joined, x)
                                                       for x in names if x in joined):
                    out += ["#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */"] + L[k:e + 1] + ["#endif"]
                    n += 1
                    skip_to = e
                else:
                    out.append(line)
            else:
                out.append(line)
        if len(out) != len(L):
            texts[p] = nl.join(out)
            write_retry(p, texts[p].encode("utf-8", errors="surrogateescape"))
    return n


def call_args(text, name):
    """True when name's parenthesised list holds number literals or expressions: a call, never a declaration. Main,
    2026-10-19: `MPO_RETURN menuPokemonOpenSub(2, 0, 0, arg1, arg2, 1);` (a macro that expands to `return`) passed
    decl_pat as "type name(...);" and was hidden under #ifndef TARGET_PC, so the PC never opened the battle's party
    menu (menuPokemonOpen_exact.c)."""
    m = re.search(re.escape(name) + r"\s*\(([^;{}]*)\)", text)
    if not m:
        return False
    for arg in m.group(1).split(","):
        a = arg.strip()
        if re.fullmatch(r"-?(?:0[xX][0-9A-Fa-f]+|\d+(?:\.\d*)?)[uUlLfF]*", a) or re.search(r"[+\-/%&|^!<>\"']", a):
            return True
    return False


def decl_pat(name):
    # a declaration, not a call: a type word (not `return`) must come before the name, and no `=` on the line
    if name not in decl_pat_cache:
        decl_pat_cache[name] = re.compile(
            r"^(\s*)((?:extern\s+)?(?:static\s+)?(?:(?:const|unsigned|signed|struct|volatile)\s+)*"
            r"(?!return\b)[A-Za-z_]\w*[\s\*]+(?:\w+[\s\*]+)*" + re.escape(name) + r"\s*\([^;{}=]*\)\s*;.*)$")
    return decl_pat_cache[name]


hdr = HDR.read_text(encoding="utf-8") if HDR.exists() else (
    "/* Canonical prototypes for the PC build (OrrePort), generated by tools/local/port/pcproto.py from each function's\n"
    " * definition. Force-included by the PC build (-include pc/prototypes.h); the GameCube build never sees it, and\n"
    " * the conflicting local declarations in the sources are hidden under #ifndef TARGET_PC. */\n"
    "#ifndef PC_PROTOTYPES_H\n#define PC_PROTOTYPES_H\n#include \"dolphin/types.h\"\n\n#endif\n")
# C-library functions (and a few names with no usable game C definition) the game declares itself with guessed types (`extern f64 atan2(f32, f32);`, `extern void
# sprintf(...)`): MWCC accepted those, but on the PC the host C library provides them, so the canonical prototype is
# the ISO C one, not a game definition.
LIBC = {
    "atan2": "double atan2(double y, double x);",
    "sprintf": "#include <stdio.h> /* sprintf: the host C library's own declaration (MinGW's is static inline) */",
    "printf": "int printf(const char* format, ...);",
    "memcpy": "#include <string.h> /* memcpy, memset, memmove, memcmp, strlen: the host C library (the game declares them with 32-bit sizes) */",
    "memset": "#include <string.h> /* memcpy, memset, memmove, memcmp, strlen: the host C library (the game declares them with 32-bit sizes) */",
    "memmove": "#include <string.h> /* memcpy, memset, memmove, memcmp, strlen: the host C library (the game declares them with 32-bit sizes) */",
    "memcmp": "#include <string.h> /* memcpy, memset, memmove, memcmp, strlen: the host C library (the game declares them with 32-bit sizes) */",
    "strlen": "#include <string.h> /* memcpy, memset, memmove, memcmp, strlen: the host C library (the game declares them with 32-bit sizes) */",
    "sin": "double sin(double x);",
    "cos": "double cos(double x);",
    "fmod": "double fmod(double x, double y);",
    "vsprintf": "#include <stdio.h> /* sprintf: the host C library's own declaration (MinGW's is static inline) */",
    # no usable C definition in the game: assembly on the GameCube (implemented by port/), or defined indented
    "fightSeqGetNromalWazaDamage": "u32 fightSeqGetNromalWazaDamage(u32 r3, u32 r4, u32 r5, u32 r6, u8 r7, u8 r8, void* r9, void* r10); /* return type on its own line in fight_seq.c (the definition finder misses that form) */",
    "peopleInfoBiosGetPtr": "struct PeopleInfoBiosEntry* peopleInfoBiosGetPtr(void* scriptObject); /* people_exact_8018F5B4.c; struct tag from game/people/people.h */",
    "winSetSequence": "void winSetSequence(struct WinSeqState* state, u16 id); /* types from game/win_sequence.h */",
    "fn_801081F8": "void fn_801081F8(struct tagWINDOW_WORK* window, u16 itemId, u16 seqId); /* types from game/win_sequence.h */",
    "GSmsgGetRect": "s32 GSmsgGetRect(u32 key); /* K&R definition in gs_msg.c (u32 parameter) */",
    "wsStatBuffChange": "u8 wsStatBuffChange(u32 r3, u32 r4, u32 r5, u32 r6); /* K&R definition with u32 parameters */",
    "fightTrainerAiGetSoubiItem": "u32 fightTrainerAiGetSoubiItem(void* ctx, u32 param); /* K&R definition (void* ctx, u32 param) */",
    "_threadSwitch": "void _threadSwitch(void); /* GS thread yield: assembly on the GameCube, port/src/gs_thread_host.c on the PC */",
    "PSVECAdd": "void PSVECAdd(const void* a, const void* b, void* sum); /* SDK paired-single assembly; port/ forwards it */",
    "heroDispatchEvent": "void heroDispatchEvent(s8 kind, u32 arg); /* defined in floor_character_exact_80116D30.cpp (extern C linkage; the definition finder skips .cpp files) */",
    "_menuFightIsUse__FP16MENU_WAZA_STATUSUs": "u32 _menuFightIsUse__FP16MENU_WAZA_STATUSUs(u8* status, u16 waza); /* assembly only (C++ name kept as the symbol); reads r3, r4 */",
    "gamedataGetStatus": "u32 gamedataGetStatus(struct GameData* ptr, u32 effect_type); /* K&R definition in gamedata.c */",
    "gamedataSetStatus": "void gamedataSetStatus(struct GameData* ptr, u32 effect_type, u32 value); /* K&R definition in gamedata.c */",
    "pokemonGetSex": "u8 pokemonGetSex(u8* ptr); /* pokemon_range_8011F5FC.c (an inline copy in pokemon_get_status_exact_8012640C.c has the same type) */",
    "GSfilterCreate": "struct GSFilter* GSfilterCreate(const u8* color); /* two live copies (one inline), same type */",
    "fn_80083BF8": "s32 fn_80083BF8(void* arena); /* scored definition in cardesavedata.c; gba_conv.c has an unscored register-style copy */",
    "fn_800849B4": "s32 fn_800849B4(s32 mode, s32 command, void* input, void* output); /* scored definition in cardesavedata.c */",
    "fn_80087C64": "u32 fn_80087C64(); /* real parameter: (const u16* expected) in cardesavedata.c; callers pass u16* or void*: unprototyped on purpose */",
    "GSresRegisterResource": "void GSresRegisterResource(); /* real parameters: (void* data, u32 group, u32 id, GSresReleaseFunc release); callers declare the callback differently: unprototyped on purpose (pointers / 32-bit ints only) */",
    "GSresAllocResourceAlign": "void* GSresAllocResourceAlign(u32 size, u32 align, u32 group, u32 id, u8 (*release)(void* data, u32 group, u32 id)); /* gs_res.c; the release callback is the 5th parameter, on the stack on x64: callers that declared it u32 or unprototyped wrote only 4 of its 8 bytes (2026-10-27: P02's item-fanfare crash) */",
    "fightFloorLoopValidFightTrainer": "void fightFloorLoopValidFightTrainer(u32 floor, void (*func)(u32, u32, u32), u32 work, u8 useThread); /* fight_floor.c */",
    "__cvt_fp2unsigned": "u32 __cvt_fp2unsigned(double value); /* MSL runtime (assembly on the GameCube, port/ on the PC): takes a double, as the MSL contract says; f32 arguments promote */",
    "figthOutPokemonGetLevel": "u8 figthOutPokemonGetLevel(void* ctx); /* the game's own spelling; defined indented in fight_out_pokemon_exact_80203A6C.c */",
}
added, skipped = [], []
# typedef names whose struct tag is the same name, in the shared headers (for --loose)
TAGGED = set()
for _h in Path("include/game").rglob("*.h"):
    TAGGED |= set(re.findall(r"^typedef\s+struct\s+(\w+)\s*\{", _h.read_text(encoding="utf-8", errors="replace"), re.M))
# Resolve every definition first: hiding inserts lines, which would shift the live-line positions for later names.
protos = []
for name in args:
    if re.search(r"\b" + re.escape(name) + r"\(", hdr) or (name in LIBC and LIBC[name] in hdr):
        skipped.append((name, "already in prototypes.h"))
        continue
    if name in LIBC:  # C-library function: on the PC the host's C library implements it, so its ISO prototype
        protos.append((name, LIBC[name]))
        continue
    defs = definition(name)
    if len(defs) != 1:
        skipped.append((name, f"{len(defs)} different live definitions: {list(defs)[:3]}"))
        continue
    (ret, params), = defs.keys()
    params = re.sub(r"\bregister\s+", "", params)  # a storage class, not part of the type
    if "--loose" in sys.argv:  # `Name*` -> `struct Name*` where a shared header says `typedef struct Name {` (same tag)
        def tag_hdr(s):
            return re.sub(r"(?<![\w])(?<!struct )(?:const\s+)?(\w+)(?=\s*\*)",
                          lambda m: (m.group(0).replace(m.group(1), "struct " + m.group(1)) if m.group(1) in TAGGED else m.group(0)), s)
        ret, params = tag_hdr(ret), ", ".join(tag_hdr(x) for x in params.split(","))
    if not PARAM.match(ret.replace("*", " * ") + " x") or not all(PARAM.match(x) for x in params.split(",") if x.strip() not in ("", "void", "...")):
        # The definition uses the file's own types. Unprototyped canonical form (C11) when that changes nothing:
        # every parameter a pointer or a 32-bit integer (no default promotion), and a basic result (a struct-pointer
        # result would conflict with the definition itself).
        plist = [x.strip() for x in params.split(",") if x.strip() not in ("", "void")]
        int32 = re.compile(r"^(?:const\s+)?(?:u32|s32|int|unsigned|unsigned int|BOOL)\s+\w+$")
        rbasic = PARAM.match(ret.replace("*", " * ") + " x")
        if "--loose" in sys.argv and "..." not in params and all("*" in x or int32.match(x) for x in plist) \
                and rbasic:
            protos.append((name, f"{ret} {name}(); /* real parameters: ({params}); its definition uses the file's own "
                                 "struct types, so unprototyped here (pointers / 32-bit ints only: no promotion changes) */"))
            continue
        skipped.append((name, f"non-basic types: {ret} ({params})"))
        continue
    protos.append((name, f"{ret} {name}({params or 'void'});"))
if "--rehide" in sys.argv:  # re-run the hiding pass for every name already in prototypes.h (new aliases / files)
    for name in re.findall(r"\b(\w+)\(", hdr.split('#include "dolphin/types.h"', 1)[-1]) + \
            [k for k, v in LIBC.items() if v.startswith("#include") and v in hdr]:
        k = hide(name)
        if k:
            print(f"REHID {name}: {k} more declarations hidden on PC")
for name, proto in protos:
    for tag in re.findall(r"\bstruct\s+(\w+)\s*\*", proto):  # forward-declare struct tags (else a parameter-list-local type)
        if f"struct {tag};" not in hdr:
            hdr = hdr.replace('#include "game/status_value.h"' + LF, '#include "game/status_value.h"' + LF + f"struct {tag};" + LF, 1)
    tail = LF + "#ifdef __cplusplus" + LF + "}" + LF  # new prototypes go inside the extern "C" block
    if tail in hdr:
        hdr = hdr.replace(tail, LF + proto + tail, 1)
    else:
        hdr = hdr.replace(LF + "#endif" + LF, LF + proto + LF + "#endif" + LF, 1) if hdr.endswith(LF + "#endif" + LF) else hdr + proto + LF
    added.append((name, proto, hide(name)))
# every struct tag a prototype names needs a forward declaration, or the type is local to that prototype and never
# matches the definition (fixed and dictionary entries too, not only the ones added in this run)
_fw = set(re.findall(r"^struct (\w+);", hdr, re.M))
for _tag in sorted(set(re.findall(r"\bstruct (\w+)\s*\*", re.sub(r"/\*.*?\*/", "", hdr, flags=re.S))) - _fw):
    hdr = hdr.replace('#include "game/status_value.h"' + LF, '#include "game/status_value.h"' + LF + f"struct {_tag};" + LF, 1)
HDR.parent.mkdir(parents=True, exist_ok=True)
HDR.write_text(hdr, encoding="utf-8")
for name, proto, n in added:
    print(f"ADDED {proto}   ({n} local declarations hidden on PC)")
for name, why in skipped:
    print(f"SKIP  {name}: {why}")
