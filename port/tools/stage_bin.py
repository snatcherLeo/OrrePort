"""Stage a build into a portable OrrePort folder's app\\ (Phase 4 step 4.1; docs/port/portable-layout.md part 6).

    python -I port/tools/stage_bin.py <OrrePort folder> [--from build-pc/firstlink] [--mingw C:/msys64/mingw64/bin]
        [--launchers build-pc/launcher] [--dry-run]

The program folder is app\\ since layout A (snatcherLeo, 2026-11-22; bin\\ before, hence the script's name, kept so every
command and note that names it keeps working). A folder that still has the old bin\\ and no app\\ is refused, nothing
copied: staging would otherwise put a second, new program folder beside the old one (firstlink.py restages snatcherLeo's folder
on every link); rename it first (bin -> app, settings -> data\\settings, system -> data\\system).

What it copies (and nothing else: it writes only inside <OrrePort folder>/app/ and the two launcher files beside it):
- orrecolo.exe and every DLL in the build folder (SDL3, webgpu_dawn, dxcompiler, dxil, nod, libpng16, libzlib1, libzstd,
  libusb-1.0), "OrrePort Adapter Setup.exe", initial_pipeline_cache.db (the shader seed: app\\, the project's call 2026-11-21),
  licenses\\;
- every further DLL those files import that Windows doesn't provide, found by reading their import tables (objdump -p),
  recursively: the MinGW runtime (libstdc++-6, libgcc_s_seh-1, libwinpthread-1) from MSYS2, and Microsoft's Visual C++
  runtime (VCRUNTIME140, VCRUNTIME140_1, MSVCP140, MSVCP140_ATOMIC_WAIT) from the Visual Studio redistributable folder on
  this PC (snatcherLeo approved shipping them in app\\, 2026-11-21). Why: a tester's PC has neither MSYS2 nor, perhaps, the
  Visual C++ redistributable, so app\\ must carry everything the exe loads (checked by starting it with MSYS2 off PATH);
- the build folder's ui\\ as app\\UI\\ (the exact capitals the portable layout reads) and Fonts\\ as app\\Fonts\\;
- Studio (--studio, default build-pc/studio): orrestudio.exe and orrestudio-cli.exe into app\\, its ui\\ as
  app\\UI\\Studio\\ and its lang\\ as app\\UI\\lang\\Studio\\, only when the DLLs it shares with OrrePort are the same files;
- the launchers (OrrePort.exe, "OrrePort Studio.exe") into the OrrePort folder itself, when built.

Rules (why): it never deletes anything (a file left over in app\\ from an older build is only listed); a file whose size
and time match is skipped (copy2 keeps the time), so restaging is quick; and it refuses to start when app\\orrecolo.exe
is running (opened for writing to test: Windows refuses that for a running exe), because a half-updated app\\ is worse
than an old one (the 2026-11-15 "LINKED but old exe" trap, the project rules).

Platforms: Windows only as written (DLL import tables, the .exe names, the VC++ redistributable). Linux (likely): the
same idea with `ldd`-style lookups and .so files next to an $ORIGIN rpath, and no Microsoft runtime; macOS (likely): the
.app bundle's Frameworks folder instead of app\\, filled by the bundle step. XD (likely): the same script with the XD
exe's name; the libraries are shared engine.
"""
import argparse
import filecmp
import glob
import os
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
EXE = "orrecolo.exe"
LAUNCHERS = ("OrrePort.exe", "OrrePort Studio.exe")
SINGLE_FILES = ("OrrePort Adapter Setup.exe", "initial_pipeline_cache.db")
VC_RUNTIME = {"vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "msvcp140_atomic_wait.dll"}


def vc_redist_folder():
    """The newest x64 Microsoft.VC14x.CRT folder of an installed Visual Studio (the redistributable copies Microsoft
    licenses for shipping beside a program), or None."""
    found = []
    for root in (r"C:\Program Files\Microsoft Visual Studio", r"C:\Program Files (x86)\Microsoft Visual Studio"):
        found += glob.glob(os.path.join(root, "*", "*", "VC", "Redist", "MSVC", "*", "x64", "Microsoft.VC14*.CRT"))
    found = [f for f in found if os.path.isfile(os.path.join(f, "vcruntime140.dll"))]
    return max(found, key=lambda f: os.path.getmtime(os.path.join(f, "vcruntime140.dll"))) if found else None


def imports(path, objdump):
    """The DLL names a PE file imports (lower case)."""
    r = subprocess.run([objdump, "-p", path], capture_output=True, text=True, errors="replace")
    return [line.split("DLL Name:", 1)[1].strip().lower() for line in r.stdout.splitlines() if "DLL Name:" in line]


def windows_has(name):
    """Windows' own DLLs (System32) and the api-ms-win-* API sets (part of Windows 10 and later)."""
    if name.startswith("api-ms-win-") or name.startswith("ext-ms-"):
        return True
    return os.path.isfile(os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", name)) and \
        name not in VC_RUNTIME  # the VC++ runtime is in System32 only when installed: never rely on it


def busy(path):
    """True when path exists and can't be opened for writing (a running exe)."""
    if not os.path.exists(path):
        return False
    try:
        with open(path, "r+b"):
            return False
    except PermissionError:
        return True


def same(a, b):
    try:
        sa, sb = os.stat(a), os.stat(b)
    except OSError:
        return False
    return sa.st_size == sb.st_size and int(sa.st_mtime) == int(sb.st_mtime)


class Stager:
    def __init__(self, dry, root):
        self.dry, self.root, self.copied, self.skipped = dry, root, [], 0

    def file(self, src, dst):
        if same(src, dst):
            self.skipped += 1
            return
        self.copied.append(os.path.relpath(dst, self.root))
        if self.dry:
            return
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        tmp = dst + ".part"
        shutil.copy2(src, tmp)
        os.replace(tmp, dst)

    def tree(self, src, dst, skip=()):
        for dirpath, dirnames, filenames in os.walk(src):
            dirnames[:] = [d for d in dirnames if os.path.relpath(os.path.join(dirpath, d), src) not in skip]
            for name in filenames:
                s = os.path.join(dirpath, name)
                self.file(s, os.path.join(dst, os.path.relpath(s, src)))


def stage(root, src, mingw, launchers, dry=False, quiet=False, studio=None):
    """Returns 0 on success, 1 when refused or a needed file is missing."""
    app_dir = os.path.join(root, "app")
    if os.path.isdir(os.path.join(root, "bin")) and not os.path.isdir(app_dir):
        print(f"stage: {root} still has the old bin\\ folder (layout A renamed it app\\, 2026-11-22); rename it first "
              "(bin -> app, settings -> data\\settings, system -> data\\system). Nothing copied")
        return 1
    for name in (EXE, "orrestudio.exe"):
        if busy(os.path.join(app_dir, name)):
            print(f"stage: {os.path.join(app_dir, name)} is running; close it, then stage again (nothing copied)")
            return 1
    if not os.path.isfile(os.path.join(src, EXE)):
        print(f"stage: no {EXE} in {src}")
        return 1
    objdump = shutil.which("objdump") or os.path.join(mingw, "objdump.exe")
    st = Stager(dry, root)

    # 1. the build's own files
    shipped = [EXE] + sorted(f for f in os.listdir(src) if f.lower().endswith(".dll"))
    for name in shipped + [f for f in SINGLE_FILES if os.path.isfile(os.path.join(src, f))]:
        st.file(os.path.join(src, name), os.path.join(app_dir, name))
    # 2. what they import that Windows lacks: MinGW's runtime from MSYS2, Microsoft's from the VS redistributable
    redist = vc_redist_folder()
    have = {n.lower() for n in shipped}
    todo, seen, missing = [os.path.join(src, n) for n in shipped], set(), []
    while todo:
        for dll in imports(todo.pop(), objdump):
            if dll in seen or dll in have or windows_has(dll):
                continue
            seen.add(dll)
            if dll in VC_RUNTIME:
                where = os.path.join(redist, dll) if redist else None
            else:
                where = os.path.join(mingw, dll)
            if where is None or not os.path.isfile(where):
                missing.append(dll)
                continue
            # the redistributable spells them in lower case; Windows doesn't mind, Linux isn't involved here
            st.file(where, os.path.join(app_dir, os.path.basename(where)))
            todo.append(where)
    # 3. licences, the UI folder (ui -> UI: the portable layout reads "UI" exactly), the fonts. The build folder's old
    # ui\fonts\ copy is skipped: every font is read from Fonts\ since 2026-11-16 (firstlink.py)
    for folder, dest, skip in (("licenses", "licenses", ()), ("ui", "UI", ("fonts",)), ("Fonts", "Fonts", ())):
        if os.path.isdir(os.path.join(src, folder)):
            st.tree(os.path.join(src, folder), os.path.join(app_dir, dest), skip)
    # the runtimes' own licence texts beside them (why: the GCC runtime exception and winpthreads' licence ask that
    # their notices travel with the DLLs; Microsoft's Redist.txt is the list of files it allows to be shipped), copied
    # from this PC's MSYS2 and Visual Studio installs, only for a runtime that was staged
    lic = os.path.join(app_dir, "licenses")
    msys = os.path.normpath(os.path.join(mingw, "..", "share", "licenses"))
    notices = []
    if seen & {"libstdc++-6.dll", "libgcc_s_seh-1.dll"}:
        notices += [(os.path.join(msys, "gcc-libs", n), f"GCC-runtime-{n}.txt") for n in ("COPYING.RUNTIME", "COPYING3", "COPYING.LIB")]
    if "libwinpthread-1.dll" in seen:
        notices.append((os.path.join(msys, "libwinpthread", "COPYING"), "winpthreads-COPYING.txt"))
    if seen & VC_RUNTIME and redist:
        vs = redist.split(os.sep + "VC" + os.sep)[0]
        notices.append((os.path.join(vs, "Licenses", "1033", "Redist.txt"), "Microsoft-Visual-C++-Redist.txt"))
    for s, name in notices:
        if os.path.isfile(s):
            st.file(s, os.path.join(lic, name))
        else:
            print(f"stage: note: licence text {s} not found")
    # 3b. Studio (its portable side merged 2026-11-21, 2cc129fbe): orrestudio.exe and orrestudio-cli.exe into app\, its
    # pictures into app\UI\Studio\ and its official words into app\UI\lang\Studio\ (the project's calls, 2026-11-21). One app\
    # serves both programs, so the DLLs Studio shares with OrrePort must be the same files: refused (Studio left out,
    # OrrePort still staged) when they differ, as a mismatched nod or Dawn would break one of the two
    studio_staged = []
    if studio and os.path.isfile(os.path.join(studio, "orrestudio.exe")):
        differ = [d for d in sorted(f for f in os.listdir(studio) if f.lower().endswith(".dll"))
                  if not os.path.isfile(os.path.join(src, d)) or not filecmp.cmp(os.path.join(src, d), os.path.join(studio, d), shallow=False)]
        if differ:
            print("stage: Studio left out: its " + ", ".join(differ) + " differ from OrrePort's (one app\\ serves both)")
        else:
            for name in ("orrestudio.exe", "orrestudio-cli.exe"):
                if os.path.isfile(os.path.join(studio, name)):
                    st.file(os.path.join(studio, name), os.path.join(app_dir, name))
                    studio_staged.append(name)
            for folder, dest in (("ui", os.path.join("UI", "Studio")), ("lang", os.path.join("UI", "lang", "Studio"))):
                if os.path.isdir(os.path.join(studio, folder)):
                    st.tree(os.path.join(studio, folder), os.path.join(app_dir, dest))
    # 3c. app\version.txt from the repo's VERSION (2026-11-23, the updater): the launcher reads it to record which version
    # it moves aside for "Go back" (orre_launcher.c, try_swap); both programs carry the same number compiled in
    version_src = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "VERSION"))
    if os.path.isfile(version_src):
        st.file(version_src, os.path.join(app_dir, "version.txt"))
    # 4. the launchers, beside app\ (the only files written outside it)
    for name in LAUNCHERS:
        if launchers and os.path.isfile(os.path.join(launchers, name)):
            if busy(os.path.join(root, name)):
                print(f"stage: {name} is running; left as it was")
                continue
            st.file(os.path.join(launchers, name), os.path.join(root, name))

    extra = []
    if os.path.isdir(app_dir):
        expected = {n.lower() for n in shipped} | seen | {f.lower() for f in SINGLE_FILES} | {"licenses", "ui", "fonts", "version.txt"} | \
            {n.lower() for n in studio_staged}
        extra = sorted(f for f in os.listdir(app_dir) if f.lower() not in expected and not f.endswith(".part"))
    if missing:
        print("stage: needed but not found: " + ", ".join(missing) +
              ("" if redist else " (no Visual Studio redistributable folder found)"))
    if not quiet or st.copied:
        print(f"stage: {len(st.copied)} file(s) {'would be ' if dry else ''}copied, {st.skipped} unchanged, into {app_dir}")
        for c in st.copied[:12]:
            print(f"  {c}")
        if len(st.copied) > 12:
            print(f"  ... and {len(st.copied) - 12} more")
    if extra:
        print("stage: in app\\ but not from this build (left alone; never deleted): " + ", ".join(extra))
    return 1 if missing else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("root", help="the OrrePort folder (app\\ is made inside it)")
    ap.add_argument("--from", dest="src", default=os.path.join(REPO, "build-pc", "firstlink"))
    ap.add_argument("--mingw", default=r"C:\msys64\mingw64\bin", help="MSYS2's mingw64 bin folder (the MinGW runtime)")
    ap.add_argument("--launchers", default=os.path.join(REPO, "build-pc", "launcher"))
    ap.add_argument("--studio", default=os.path.join(REPO, "build-pc", "studio"),
                    help="Studio's build folder ('' leaves Studio out)")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    sys.exit(stage(os.path.abspath(a.root), a.src, a.mingw, a.launchers, a.dry_run, studio=a.studio or None))


if __name__ == "__main__":
    main()
