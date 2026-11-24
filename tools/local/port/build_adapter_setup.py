r"""Build "OrrePort Adapter Setup.exe", the one-click GameCube adapter setup (2026-11-13; snatcherLeo: "Separate helper (1)").

    python -I tools/local/port/build_adapter_setup.py [--out build-pc/adapter-setup] [--mingw C:/msys64/mingw64/bin]

Why a script of its own, not autotools: libwdi's own build is autotools (./bootstrap.sh, ./configure), which this PC's
MSYS2 doesn't have, and installing it could pull in an MSYS2 upgrade (the same reason libwebp was built by hand,
docs/licensing.md). libwdi is a handful of C files, so this script does what its Makefile.am does, for 64-bit only:
  1. writes libwdi's config.h (WinUSB only: the WDK 8.0 redistributable's two helper DLLs are embedded);
  2. builds libwdi's installer_x64.exe (the part libwdi runs to install a driver);
  3. builds and runs libwdi's embedder, which packs that installer, the two DLLs and the .inf / .cat templates into
     embedded.h;
  4. compiles libwdi and port/adapter_setup/orre_adapter_setup.c into one program, statically (no MinGW DLLs next to
     it), with its manifest and version details, as -mwindows (no console window flashes up).
Then the program and its licences are copied to build-pc/firstlink/ (next to orrecolo.exe, where the 0.1 folder keeps
it; bin\ in the full layout, the project's orreport-plan section 8) and licenses/.

Inputs:
  extern/libwdi-1.5.1/  libwdi's source, tag v1.5.1 (github.com/pbatard/libwdi), unchanged (*vendored 2026-11-24*,
      snatcherLeo's option C; it was in decomp/_external/)
  deps/dl/wdk8-redist/x/Windows Kits/8.0/redist/wdf/ (or decomp/_external/dl/... on this PC; never committed)
      Microsoft's WDK 8.0 redistributable components (download.microsoft.com wdfcoinstaller.msi, signed by Microsoft,
      unpacked with "msiexec /a" which installs nothing): WdfCoInstaller01011.dll and winusbcoinstaller2.dll (x64),
      plus its License.rtf
Platforms: Windows only (the program is). Linux and macOS get their own small setup later, if they need one at all
(snatcherLeo, 2026-11-13: "we'd have to make linux and Mac alternatives, but that'll be later, based on how the OS's can best
interpret the Gamecube usb adapter"; likely: a udev rule on Linux, nothing on macOS).
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MAIN = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
LIBWDI = os.path.join(MAIN, "extern", "libwdi-1.5.1")
LIBWDI_VERSION = "1.5.1"
# Microsoft's redistributable: downloaded into deps/ (tools/fetch_deps.py), else this PC's older decomp/_external/
_WDK_PARTS = ("dl", "wdk8-redist", "x", "Windows Kits", "8.0")
WDK = next((p for p in (os.path.join(MAIN, "deps", *_WDK_PARTS),
                        os.path.normpath(os.path.join(MAIN, "..", "_external", *_WDK_PARTS))) if os.path.isdir(p)),
           os.path.join(MAIN, "deps", *_WDK_PARTS))
SRC = os.path.join(MAIN, "port", "adapter_setup")
EXE = "OrrePort Adapter Setup.exe"

LIB_SOURCES = ["logging.c", "tokenizer.c", "vid_data.c", "pki.c", "libwdi_dlg.c", "libwdi.c"]


def run(cmd, cwd=None, env=None):
    print("  " + " ".join(f'"{c}"' if " " in c else c for c in cmd))
    r = subprocess.run(cmd, cwd=cwd, env=env)
    if r.returncode != 0:
        sys.exit(f"failed ({r.returncode}): {cmd[0]}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=os.path.join(MAIN, "build-pc", "adapter-setup"))
    ap.add_argument("--mingw", default="C:/msys64/mingw64/bin")
    ap.add_argument("--dest", default=os.path.join(MAIN, "build-pc", "firstlink"),
                    help="where the finished program and its licences go (the play build's folder)")
    a = ap.parse_args()
    for need in (os.path.join(LIBWDI, "libwdi", "libwdi.c"),
                 os.path.join(WDK, "redist", "wdf", "x64", "WdfCoInstaller01011.dll"),
                 os.path.join(WDK, "redist", "wdf", "x64", "winusbcoinstaller2.dll")):
        if not os.path.isfile(need):
            sys.exit(f"missing {need}: see this script's notes for where it comes from")
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    env = dict(os.environ)
    env["PATH"] = a.mingw + os.pathsep + env.get("PATH", "")
    gcc = os.path.join(a.mingw, "gcc.exe")
    windres = os.path.join(a.mingw, "windres.exe")
    wdi = os.path.join(LIBWDI, "libwdi")

    # 1. config.h: what libwdi's configure would write for "--with-wdkdir=... --disable-32bit" (configure.ac); paths in
    #    Windows form, as the embedder joins them with backslashes
    def cpath(p):
        return p.replace("/", "\\").replace("\\", "\\\\")
    config = (
        "/* libwdi's config.h for OrrePort Adapter Setup (written by tools/local/port/build_adapter_setup.py) */\n"
        "#define _GNU_SOURCE 1\n"
        f'#define WDK_DIR "{cpath(WDK)}"\n'
        "#define WDF_VER 1011\n"
        '#define COINSTALLER_DIR "wdf"\n'
        '#define X64_DIR "x64"\n'
        "#define OPT_M64 1\n"
        "#define ENABLE_LOGGING 1\n"
        f'#define SOLUTIONDIR "{cpath(out)}"\n')
    with open(os.path.join(out, "config.h"), "w", newline="\n") as f:
        f.write(config)

    # 2. libwdi's installer (64-bit, static)
    print("installer_x64.exe")
    run([gcc, "-O2", "-m64", "-D_WIN64", "-I", out, "-I", wdi, "-I", LIBWDI, os.path.join(wdi, "installer.c"),
         "-o", os.path.join(out, "installer_x64.exe"), "-static", "-lsetupapi", "-lnewdev", "-lole32"], env=env)

    # 3. the embedder, run from libwdi's folder (the .inf / .cat templates are named relative to it)
    print("embedded.h")
    run([gcc, "-O2", "-I", out, "-I", wdi, "-I", LIBWDI, os.path.join(wdi, "embedder.c"),
         "-o", os.path.join(out, "embedder.exe")], env=env)
    emb = os.path.join(out, "embedded.h")
    if os.path.exists(emb):
        os.remove(emb)  # our own build output (the embedder skips an unchanged one; always rebuild it)
    run([os.path.join(out, "embedder.exe"), emb], cwd=wdi, env=env)

    # 4. the program: libwdi + ours, static, a window program (-mwindows), with its manifest and version details
    print(EXE)
    res = os.path.join(out, "orre_adapter_setup_res.o")
    run([windres, "-I", SRC, os.path.join(SRC, "orre_adapter_setup.rc"), "-O", "coff", "-o", res], env=env)
    cmd = [gcc, "-O2", "-m64", "-D_WIN64", "-DNDEBUG", f'-DORRE_LIBWDI_VERSION="{LIBWDI_VERSION}"',
           "-I", out, "-I", wdi, "-I", LIBWDI]
    cmd += [os.path.join(wdi, s) for s in LIB_SOURCES]
    cmd += [os.path.join(SRC, "orre_adapter_setup.c"), res, "-o", os.path.join(out, EXE), "-mwindows", "-static",
            "-lsetupapi", "-lole32", "-lntdll", "-lcomctl32", "-lshell32", "-ladvapi32", "-luuid"]
    run(cmd, env=env)

    # 5. into the play build's folder, with the licences: libwdi's LGPL v3 (and the GPL v3 text it builds on),
    #    Microsoft's terms for the two redistributable DLLs, and where libwdi's source is
    source_note = os.path.join(out, "libwdi-SOURCE.txt")
    with open(source_note, "w", newline="\r\n") as f:
        f.write("OrrePort Adapter Setup.exe contains libwdi " + LIBWDI_VERSION + " (LGPL v3), unchanged.\n"
                "Its source: https://github.com/pbatard/libwdi (tag v" + LIBWDI_VERSION + ").\n"
                "It also contains Microsoft's WinUSB co-installers from the WDK 8.0 redistributable components\n"
                "(WdfCoInstaller01011.dll, winusbcoinstaller2.dll), distributed under Microsoft's terms in\n"
                "Microsoft-WDF-1.11-redistributable-License.rtf.\n")
    dest = os.path.abspath(a.dest)
    if os.path.isdir(dest):
        shutil.copy2(os.path.join(out, EXE), os.path.join(dest, EXE))
        lic = os.path.join(dest, "licenses")
        os.makedirs(lic, exist_ok=True)
        shutil.copyfile(os.path.join(LIBWDI, "COPYING-LGPL"), os.path.join(lic, "libwdi-COPYING-LGPL-3.txt"))
        shutil.copyfile(os.path.join(LIBWDI, "COPYING"), os.path.join(lic, "libwdi-COPYING-GPL-3.txt"))
        shutil.copyfile(os.path.join(WDK, "redist", "wdf", "License.rtf"),
                        os.path.join(lic, "Microsoft-WDF-1.11-redistributable-License.rtf"))
        shutil.copyfile(source_note, os.path.join(lic, "libwdi-SOURCE.txt"))
        print(f"copied {EXE} and its licences into {dest}")
    print("done:", os.path.join(out, EXE))


if __name__ == "__main__":
    main()
