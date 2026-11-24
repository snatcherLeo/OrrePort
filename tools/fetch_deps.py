"""Download the third-party files OrrePort's build ships but git doesn't keep: the fonts, the libusb DLL and Microsoft's
driver redistributable for the adapter setup (2026-11-24; snatcherLeo's option C: the small source libraries are
vendored in extern/, these larger binaries are fetched).

    python -I tools/fetch_deps.py            download whatever is missing or wrong into deps/
    python -I tools/fetch_deps.py --check    only check what is there (exit 1 if anything is missing or wrong)

Why a script and pinned checksums: every file comes from its official source (Google's fonts repository, the Noto
projects, the font authors' releases, libusb's release, Microsoft's download server) at a fixed version, and must match
the SHA-256 listed in tools/deps.json byte for byte, so every builder ships exactly the files the release ships. A file
that doesn't match is refused and deleted from the download folder, never used. Nothing is installed anywhere: files
land in deps/ inside the repo (git-ignored), where port/tools/firstlink.py looks for them.

tools/deps.json, one entry per file:
  {"dest": "fonts/inter/Inter-Variable.ttf", "url": "...", "sha256": "...", "size": n,
   "archive_member": null | "path inside the downloaded archive", "archive_sha256": null | "...",
   "msi": false | true}
A plain entry is the file itself. An entry with archive_member is a member of a .zip / .7z / .tar.* download (the
archive is checked against archive_sha256 when given, then the member against sha256). An entry with "msi": true is a
member of a Windows Installer package, unpacked with "msiexec /a" (an administrative unpack: it installs nothing).

Platforms: plain Python 3 (urllib, zipfile, tarfile, hashlib). .7z archives are read with the system's tar (Windows 10
and later ship bsdtar, which reads 7z; Linux / macOS: bsdtar or 7z, likely). The .msi entry is Windows only (the adapter
setup it feeds is Windows only too).
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
import tarfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEPS = os.path.join(ROOT, "deps")
MANIFEST = os.path.join(ROOT, "tools", "deps.json")
CACHE = os.path.join(DEPS, "_downloads")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def download(url, dest):
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    tmp = dest + ".part"
    req = urllib.request.Request(url, headers={"User-Agent": "OrrePort-fetch-deps"})
    with urllib.request.urlopen(req, timeout=120) as r, open(tmp, "wb") as f:
        shutil.copyfileobj(r, f)
    os.replace(tmp, dest)


def fetch_archive(entry):
    """the archive for an entry, downloaded once into deps/_downloads/ and checked against archive_sha256"""
    # archive_name: for a link with no file name in it (Microsoft's go.microsoft.com redirect)
    name = entry.get("archive_name") or entry["url"].rstrip("/").rsplit("/", 1)[-1]
    path = os.path.join(CACHE, name)
    want = entry.get("archive_sha256")
    if not (os.path.isfile(path) and (not want or sha256(path) == want)):
        print(f"  downloading {entry['url']}")
        download(entry["url"], path)
        if want and sha256(path) != want:
            os.remove(path)
            raise SystemExit(f"REFUSED: {name} doesn't match its pinned checksum (removed)")
    return path


def extract_member(archive, member, out):
    os.makedirs(os.path.dirname(out), exist_ok=True)
    low = archive.lower()
    if low.endswith(".zip"):
        with zipfile.ZipFile(archive) as z, z.open(member) as src, open(out, "wb") as dst:
            shutil.copyfileobj(src, dst)
        return
    if low.endswith((".tar.gz", ".tgz", ".tar.xz", ".tar.bz2")):
        with tarfile.open(archive) as t:
            src = t.extractfile(member)
            if src is None:
                raise SystemExit(f"{member} not in {archive}")
            with open(out, "wb") as dst:
                shutil.copyfileobj(src, dst)
        return
    # .7z and anything else: bsdtar. On Windows that is System32's tar.exe, named in full: a plain "tar" can resolve to
    # Git Bash's GNU tar, which reads no 7z and takes "C:" in a path for a remote host (found 2026-11-24 on the first
    # run). Linux / macOS: bsdtar when installed (likely; GNU tar can't read 7z)
    tar = "tar"
    if os.name == "nt":
        tar = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "tar.exe")
    elif shutil.which("bsdtar"):
        tar = "bsdtar"
    with tempfile.TemporaryDirectory(dir=CACHE) as tmp:
        subprocess.run([tar, "-xf", archive, "-C", tmp, member], check=True)
        shutil.copyfile(os.path.join(tmp, *member.split("/")), out)


def extract_msi(archive, member, out):
    if os.name != "nt":
        raise SystemExit(f"{os.path.basename(archive)}: a Windows Installer package, unpacked on Windows only")
    with tempfile.TemporaryDirectory(dir=CACHE) as tmp:
        # an administrative unpack: copies the package's files into tmp, installs and registers nothing
        subprocess.run(["msiexec", "/a", archive, "/qn", f"TARGETDIR={tmp}"], check=True)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        shutil.copyfile(os.path.join(tmp, *member.split("/")), out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    with open(MANIFEST, encoding="utf-8") as f:
        manifest = json.load(f)
    # "sources" names each pinned place once (a repository at a commit, a release); an entry's url "name:rest" is that
    # source's address followed by rest, so a long commit-pinned prefix isn't repeated 150 times
    sources = manifest.get("sources", {})
    entries = manifest["files"]
    for e in entries:
        key, sep, rest = e["url"].partition(":")
        if sep and key in sources:
            e["url"] = sources[key] + rest
    bad = 0
    for e in entries:
        dest = os.path.join(DEPS, *e["dest"].split("/"))
        if os.path.isfile(dest) and sha256(dest) == e["sha256"]:
            continue
        if a.check:
            print(f"missing or different: deps/{e['dest']}")
            bad += 1
            continue
        print(f"deps/{e['dest']}")
        if e.get("msi"):
            extract_msi(fetch_archive(e), e["archive_member"], dest)
        elif e.get("archive_member"):
            extract_member(fetch_archive(e), e["archive_member"], dest)
        else:
            download(e["url"], dest)
        if sha256(dest) != e["sha256"]:
            os.remove(dest)
            print(f"  REFUSED: doesn't match its pinned checksum (removed)")
            bad += 1
    print(f"{len(entries) - bad} of {len(entries)} files in place" + ("" if not bad else f"; {bad} missing or refused"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
