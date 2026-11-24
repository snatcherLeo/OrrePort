"""Make one OrrePort release for the updater: the zip of app\\, update.txt, and update.sig (2026-11-23).

Why: both programs update from three files on the public repo's GitHub Releases (docs/port/updater-plan.md,
port/src/pc_update.c): update.txt ("key = value": version, the zip's name, size and SHA-512, what's new per language),
update.sig (its Ed25519 signature, made by build-pc/orre_sign.exe with snatcherLeo's secret key), and the zip holding the new
app\\ only (never launchers, saves, mods or settings).

    python -I tools/local/port/make_release.py <app folder> <version> <out folder> <secret key file> [notes file]
        [--full <clean OrrePort folder>]

<app folder>: the staged program folder to ship (the play folder's app\\). <notes file>: optional, "notes.<lang> = ..."
lines (one per language, "\\n" for a line break); English is required when given. The secret key never leaves its file
(orre_sign reads it); this script only hands its path over.

The files a release carries (*decided 2026-11-24*, Main with the docs session, for the release page):
  OrrePort-<version>-Windows.zip          the whole portable folder, for a first download (--full): the one players take
  OrrePort-<version>-Windows-update.zip   app\\ only, the one update.txt names and the update button fetches
  update.txt, update.sig                  the update check's list and its signature
--full <folder>: zips a CLEAN OrrePort folder (the launchers, app\\ from port/tools/stage_bin.py, the empty folders and
README; never a play folder) under a top folder "OrrePort\\". It refuses, and writes nothing, if the folder holds anything
personal or from the disc: saves (.gci, .orresave, a Saves folder with files), disc images, settings.ini, logs or
data\\system\\ state (a play folder has all of these).

Platforms: plain Python (zipfile, hashlib); orre_sign is built for each system. XD: the same (one app\\ for both games).
"""
import hashlib
import os
import subprocess
import sys
import zipfile


PERSONAL_EXT = (".gci", ".orresave", ".iso", ".gcm", ".rvz", ".ciso", ".gcz", ".wbfs", ".nkit", ".log", ".dmp")


def personal_files(folder):
    """anything in a would-be release folder that must never ship: saves, disc images, settings, logs, state"""
    bad = []
    for root, _dirs, files in os.walk(folder):
        rel_root = os.path.relpath(root, folder).replace(os.sep, "/")
        for f in files:
            rel = f if rel_root == "." else f"{rel_root}/{f}"
            low = rel.lower()
            if (low.endswith(PERSONAL_EXT) or f.lower() == "settings.ini" or "/saves/" in f"/{low}"
                    or low.startswith("data/system/") or low.startswith("data/settings/") or "/logs/" in f"/{low}"):
                bad.append(rel)
    return bad


def main():
    args = sys.argv[1:]
    full = None
    if "--full" in args:
        i = args.index("--full")
        if i + 1 >= len(args):
            print(__doc__)
            return 2
        full = args[i + 1]
        del args[i:i + 2]
    if len(args) not in (4, 5):
        print(__doc__)
        return 2
    app, version, out, key = args[:4]
    notes = open(args[4], encoding="utf-8").read().strip() + "\n" if len(args) == 5 else ""
    if not os.path.isdir(app):
        print(f"no app folder at {app}")
        return 1
    if full:
        bad = personal_files(full)
        if bad:
            print(f"REFUSED: {full} holds files that must never ship ({len(bad)}), e.g.: " + ", ".join(bad[:5]))
            return 1
    os.makedirs(out, exist_ok=True)
    if full:
        fname = f"OrrePort-{version}-Windows.zip"
        with zipfile.ZipFile(os.path.join(out, fname), "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for root, dirs, files in os.walk(full):
                rel_root = os.path.relpath(root, full)
                if not files and not dirs:  # keep the empty folders the player fills (the disc's ROM folder)
                    z.writestr("OrrePort/" + rel_root.replace(os.sep, "/") + "/", "")
                for f in sorted(files):
                    p = os.path.join(root, f)
                    z.write(p, "OrrePort/" + os.path.relpath(p, full).replace(os.sep, "/"))
        print(f"first download: {fname} ({os.path.getsize(os.path.join(out, fname)) / 1048576:.1f} MB)")
    name = f"OrrePort-{version}-Windows-update.zip"
    zpath = os.path.join(out, name)
    # deflate: the game's program is ~280 MB and compresses well; the updater unpacks with zlib (pc_update.c)
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for root, _dirs, files in os.walk(app):
            for f in sorted(files):
                full = os.path.join(root, f)
                rel = os.path.relpath(full, app).replace(os.sep, "/")
                z.write(full, "app/" + rel)
    sha = hashlib.sha512()
    with open(zpath, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            sha.update(block)
    size = os.path.getsize(zpath)
    txt = os.path.join(out, "update.txt")
    with open(txt, "w", encoding="utf-8", newline="\n") as f:
        f.write(f"version = {version}\nfile = {name}\nsize = {size}\nsha512 = {sha.hexdigest()}\n{notes}")
    sign = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "build-pc", "orre_sign.exe")
    r = subprocess.run([sign, "sign", key, txt, os.path.join(out, "update.sig")])
    if r.returncode != 0:
        print("signing failed")
        return 1
    print(f"release {version}: {name} ({size / 1048576:.1f} MB), update.txt, update.sig in {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
