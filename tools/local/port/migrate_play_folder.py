r"""Copy snatcherLeo's old play folder (build-pc\firstlink, the flat layout) into the new portable folder, copy only
(Phase 4 step 4.1; docs/port/portable-layout.md part 5; snatcherLeo's approval 6, 2026-11-21).

    python -I tools/local/port/migrate_play_folder.py <old folder> <new OrrePort folder> --plan     (lists, writes nothing)
    python -I tools/local/port/migrate_play_folder.py <old folder> <new OrrePort folder> --us-disc <iso>   (copies)
    python -I tools/local/port/migrate_play_folder.py <old folder> <new OrrePort folder> --verify   (re-checks only)

Why copy only: snatcherLeo's saves, settings and disc art are irreplaceable, and the decided rule is that nothing is moved or
deleted; the old folder stays exactly as it is (and still playable) until snatcherLeo decides about it. So this tool:
- never deletes, moves or changes anything in the old folder (it only reads it);
- never overwrites: it refuses before copying anything if any destination file already exists;
- refuses while an orrecolo.exe runs from the old folder, or while snatcherLeo's play marker is live;
- writes the whole plan (every file, where it goes, how, its size and SHA-1) to
  <new>\data\system\logs\migration_<stamp>.txt BEFORE copying, then the result after;
- copies each small file byte for byte (shutil.copy2 keeps the times) under a ".part" name, then renames it, and
  checks the copy's SHA-1 against the source's. Why byte-identical matters: slot.ini's spot= line holds the save's
  SHA-1 (a changed byte drops the saved spot), extras.ini carries the save's fingerprint (the long names), and Undo
  points at a Backups\ folder by name (docs/port/portable-layout.md part 5);
- saves are always real copies, never hard links: the game rewrites save files in place, and a link would change the
  old folder's copy too;
- disc images and the texture pack (files the game only reads) are HARD LINKS, a second name for the same data, not a
  copy and not a move: no 1.4 GB copies, and nothing in the old place changes. NTFS allows at most 1,024 names per file:
  a link that fails for that reason is reported, not replaced by a copy (re-run with the disc from a copy that has room).
  The US disc comes from --us-disc (the old folder's ROM\ is empty: its launcher passed --disc), e.g. one of the
  _run3\discs\disc_N.iso copies, because orig\'s US image already has 1,024 names (verified 2026-11-21).

The mapping (the decided tree, the project notes section 8, with the project's six calls of 2026-11-21; layout A since
2026-11-22, snatcherLeo: OrrePort's own folders app\ and data\ (data\settings\, data\system\), the player's capitalized):
  Colosseum\Saves\**            -> Pokemon Colosseum\Saves\**           copy (every slot with slot.ini, extras.ini,
                                                                         region_header.bin, Backups\, Autosave\)
  Colosseum\Recordings\**       -> data\system\recordings\Pokemon Colosseum\**  copy (beside the logs since 2026-11-21, snatcherLeo)
  Colosseum\ROM Other Regions\* -> Pokemon Colosseum\ROM Other Regions\ hard link
  (--us-disc)                   -> Pokemon Colosseum\ROM\Pokemon Colosseum (USA).iso   hard link
  Disc labels\**                -> Pokemon Colosseum\Disc Art\**        copy (snatcherLeo: the game's own folder, named
                                                                         "Disc Art" on 2026-11-21; was Art\)
  user\textures\GC6E01\**       -> Pokemon Colosseum\Textures\**        hard link per file (snatcherLeo: "Put the textures
                                                                         subfolder")
  screenshots\**                -> Pokemon Colosseum\Screenshots\**     copy
  Reports\**                    -> Pokemon Colosseum\Reports\**         copy
  user\settings.ini, imgui.ini, keyboard_bindings.dat, *.controller, controller_ports.dat -> data\settings\   copy
  user\profiles\**              -> data\settings\profiles\**                 copy
  checkpoints\**                -> data\system\checkpoints\**                copy
  logs\Cheats log.txt           -> data\system\logs\Cheats log.txt           copy (the player's record of every cheat)
Left out on purpose: cache\ (rebuilt), the run logs (history stays in the old folder), Colosseum\disc-check.txt
(re-made in seconds, since 2026-11-21 in data\system\cache\Pokemon Colosseum\), user\USA\Card A\ (the legacy card, copied into Slot 1 on 2026-11-01), the build's own files
(exe, DLLs, ui\, Fonts\, objects: app\ is filled by port/tools/stage_bin.py), test files. Anything else found in the
old folder's data places is listed as "not copied (unknown)" for a person to decide.

Platforms: Windows as written (NTFS hard links, the paths). Linux / macOS (likely): the same, with os.link working on
ext4 / APFS (far higher link limits). XD (likely): the same tool with XD's game folder name; nothing Colosseum-specific
but the folder names.
"""
import argparse
import datetime
import hashlib
import os
import shutil
import subprocess
import sys

GAME = "Pokemon Colosseum"
SETTINGS_FILES = ("settings.ini", "imgui.ini", "keyboard_bindings.dat", "controller_ports.dat")
MARKER = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..",
                                       ".orre_player_playing"))


def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def walk(base):
    for dirpath, _dirs, files in os.walk(base):
        for name in sorted(files):
            yield os.path.join(dirpath, name)


def build_plan(old, new, us_disc):
    """[(how, source, destination)], how = copy | link; and [unknown source]"""
    plan, unknown = [], []

    def tree(src_rel, dst_rel, how):
        src = os.path.join(old, src_rel)
        if os.path.isdir(src):
            for p in walk(src):
                plan.append((how, p, os.path.join(new, dst_rel, os.path.relpath(p, src))))

    tree(os.path.join("Colosseum", "Saves"), os.path.join(GAME, "Saves"), "copy")
    tree(os.path.join("Colosseum", "Recordings"), os.path.join("data", "system", "recordings", "Pokemon Colosseum"), "copy")  # pc_paths.c PC_PLACE_RECORDINGS
    tree(os.path.join("Colosseum", "ROM Other Regions"), os.path.join(GAME, "ROM Other Regions"), "link")
    if us_disc:
        # named as the disc, not as the copy it came from (disc_N.iso): the game finds it by its game ID either way
        plan.append(("link", os.path.abspath(us_disc), os.path.join(new, GAME, "ROM", "Pokemon Colosseum (USA).iso")))
    tree("Disc labels", os.path.join(GAME, "Disc Art"), "copy")  # pc_paths.c PC_PLACE_ART (was "Art" until 2026-11-21)
    tree(os.path.join("user", "textures", "GC6E01"), os.path.join(GAME, "Textures"), "link")
    tree("screenshots", os.path.join(GAME, "Screenshots"), "copy")
    tree("Reports", os.path.join(GAME, "Reports"), "copy")
    user = os.path.join(old, "user")
    if os.path.isdir(user):
        for name in sorted(os.listdir(user)):
            p = os.path.join(user, name)
            if os.path.isfile(p) and (name in SETTINGS_FILES or name.endswith(".controller")):
                plan.append(("copy", p, os.path.join(new, "data", "settings", name)))
            elif name in ("profiles", "textures", "USA") or name == ".write_test" or name.startswith("handover-"):
                continue
            else:
                unknown.append(p)
    tree(os.path.join("user", "profiles"), os.path.join("data", "settings", "profiles"), "copy")
    tree("checkpoints", os.path.join("data", "system", "checkpoints"), "copy")
    cheats = os.path.join(old, "logs", "Cheats log.txt")
    if os.path.isfile(cheats):
        plan.append(("copy", cheats, os.path.join(new, "data", "system", "logs", "Cheats log.txt")))
    colo = os.path.join(old, "Colosseum")
    if os.path.isdir(colo):
        for name in sorted(os.listdir(colo)):
            if name not in ("Saves", "Recordings", "ROM", "ROM Other Regions", "disc-check.txt"):
                unknown.append(os.path.join(colo, name))
    rom = os.path.join(colo, "ROM")
    if os.path.isdir(rom):
        unknown += [os.path.join(rom, n) for n in os.listdir(rom)]  # the old ROM\ is empty; anything there: ask
    return plan, unknown


def old_game_running(old):
    exe = os.path.normcase(os.path.abspath(os.path.join(old, "orrecolo.exe")))
    r = subprocess.run(["powershell", "-NoProfile", "-Command",
                        "Get-Process orrecolo -ErrorAction SilentlyContinue | ForEach-Object { $_.Path }"],
                       capture_output=True, text=True)
    return any(os.path.normcase(line.strip()) == exe for line in r.stdout.splitlines())


def play_marker_live():
    if not os.path.isfile(MARKER):
        return False
    try:
        pid = int(open(MARKER, encoding="utf-8", errors="replace").read().split()[0])
    except (ValueError, IndexError, OSError):
        return True  # unreadable: treat as live, to be safe
    r = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH"], capture_output=True, text=True)
    return str(pid) in r.stdout


def describe(plan, unknown, old, new):
    lines = []
    total = {"copy": [0, 0], "link": [0, 0]}
    groups = {}
    for how, src, dst in plan:
        size = os.path.getsize(src)
        total[how][0] += 1
        total[how][1] += size
        dst_dir = os.sep.join(os.path.relpath(os.path.dirname(dst), new).split(os.sep)[:2])
        src_rel = os.path.relpath(os.path.dirname(src), old)
        src_dir = "(--us-disc)" if src_rel.startswith("..") else os.sep.join(src_rel.split(os.sep)[:2])
        g = groups.setdefault((how, src_dir, dst_dir), [0, 0])
        g[0] += 1
        g[1] += size
    for (how, src, dst), (n, size) in sorted(groups.items(), key=lambda kv: kv[0][2]):
        lines.append(f"  {how:4}  {n:5} file(s) {size / 1e6:9.1f} MB   {src}\\  ->  {dst}\\")
    lines.append(f"  total: {total['copy'][0]} copied ({total['copy'][1] / 1e6:.1f} MB), "
                 f"{total['link'][0]} hard-linked ({total['link'][1] / 1e9:.2f} GB, no extra space)")
    for u in unknown:
        lines.append(f"  not copied (unknown, for a person to decide): {os.path.relpath(u, old)}")
    return lines


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("old")
    ap.add_argument("new")
    ap.add_argument("--us-disc", help="the US disc image to hard-link into the new ROM\\ (one with fewer than 1,024 names)")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--plan", action="store_true", help="list what would be copied; write nothing")
    mode.add_argument("--verify", action="store_true", help="re-check an earlier migration; write nothing but its log")
    a = ap.parse_args()
    old, new = os.path.abspath(a.old), os.path.abspath(a.new)
    if not os.path.isdir(os.path.join(old, "Colosseum")):
        sys.exit(f"{old} has no Colosseum\\ folder: not an old play folder")
    plan, unknown = build_plan(old, new, a.us_disc)
    print(f"migration: {old}\n        -> {new}")
    print("\n".join(describe(plan, unknown, old, new)))
    if a.plan:
        clash = [d for _h, _s, d in plan if os.path.exists(d)]
        print(f"plan only: nothing written. {len(clash)} destination file(s) already exist" +
              (f" (e.g. {clash[0]}): a real run would refuse" if clash else ""))
        return

    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    log_dir = os.path.join(new, "data", "system", "logs")
    if a.verify:
        # the result is kept beside the migration's own log (migration_verify_<stamp>.txt), so the check stays on record
        problems = []
        for how, src, dst in plan:
            if not os.path.isfile(dst):
                problems.append(f"MISSING {dst}")
            elif how == "link" and not os.path.samefile(src, dst):
                problems.append(f"NOT THE SAME FILE {dst}")
            elif how == "copy" and sha1(src) != sha1(dst):
                problems.append(f"DIFFERS {dst}")
        result = (f"verify: {len(plan)} file(s) ({sum(1 for p in plan if p[0] == 'copy')} copies re-hashed against "
                  f"their sources, {sum(1 for p in plan if p[0] == 'link')} links checked to be the same file), "
                  f"{len(problems)} difference(s)")
        for p in problems:
            print(f"  {p}")
        print(result)
        os.makedirs(log_dir, exist_ok=True)
        with open(os.path.join(log_dir, f"migration_verify_{stamp}.txt"), "w", encoding="utf-8") as log:
            log.write(f"OrrePort play folder migration check, {stamp}\nfrom {old}\nto   {new}\n" +
                      "".join(p + "\n" for p in problems) + result + "\n")
        sys.exit(1 if problems else 0)

    # refusals, before anything is written
    if old_game_running(old):
        sys.exit("refused: an orrecolo.exe from the old folder is running (close the game first); nothing written")
    if play_marker_live():
        sys.exit("refused: snatcherLeo's play marker is live (he is playing); nothing written")
    clash = [d for _h, _s, d in plan if os.path.exists(d)]
    if clash:
        sys.exit(f"refused: {len(clash)} destination file(s) already exist (e.g. {clash[0]}); nothing written, "
                 "nothing overwritten")
    if a.us_disc and not os.path.isfile(a.us_disc):
        sys.exit(f"refused: --us-disc {a.us_disc} not found")

    # the plan file, with every source's size and SHA-1, before copying (links: size and file id; their data is the
    # same file, so a hash would only re-read gigabytes)
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, f"migration_{stamp}.txt")
    hashes = {}
    with open(log_path, "w", encoding="utf-8") as log:
        log.write(f"OrrePort play folder migration (copy only), {stamp}\nfrom {old}\nto   {new}\n\n")
        log.write("\n".join(describe(plan, unknown, old, new)) + "\n\nPLAN (how, size, sha1 or -, source -> destination)\n")
        for how, src, dst in plan:
            h = sha1(src) if how == "copy" else "-"
            hashes[src] = h
            log.write(f"{how}\t{os.path.getsize(src)}\t{h}\t{os.path.relpath(src, old)}\t->\t{os.path.relpath(dst, new)}\n")
        log.flush()

        # the copies and links
        failed = []
        for how, src, dst in plan:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            try:
                if how == "link":
                    os.link(src, dst)
                else:
                    tmp = dst + ".part"
                    shutil.copy2(src, tmp)
                    os.replace(tmp, dst)
            except OSError as e:
                failed.append((dst, str(e)))
                log.write(f"FAILED\t{os.path.relpath(dst, new)}\t{e}\n")

        # the check: every copy re-hashed against the plan, every link the same file, file counts per slot folder
        bad = []
        for how, src, dst in plan:
            if not os.path.isfile(dst):
                continue
            if how == "link":
                if not os.path.samefile(src, dst):
                    bad.append(dst)
            elif sha1(dst) != hashes[src]:
                bad.append(dst)
        slots_old = os.path.join(old, "Colosseum", "Saves")
        slots_new = os.path.join(new, GAME, "Saves")
        count_diff = []
        if os.path.isdir(slots_old):
            for name in sorted(os.listdir(slots_old)):
                n_old = sum(1 for _ in walk(os.path.join(slots_old, name))) if os.path.isdir(os.path.join(slots_old, name)) else 1
                p_new = os.path.join(slots_new, name)
                n_new = (sum(1 for _ in walk(p_new)) if os.path.isdir(p_new) else 1) if os.path.exists(p_new) else 0
                if n_old != n_new:
                    count_diff.append(f"{name}: {n_old} old, {n_new} new")
        copied = [p for p in plan if p[0] == "copy"]
        result = (f"migrated {len(plan) - len(failed)} of {len(plan)} file(s) "
                  f"({len(copied)} copied, {sum(os.path.getsize(s) for _h, s, _d in copied) / 1e6:.1f} MB; "
                  f"{len(plan) - len(copied)} hard-linked), {len(bad)} difference(s), {len(failed)} failure(s), "
                  f"{len(count_diff)} slot folder(s) with a different file count")
        log.write("\nRESULT\n" + result + "\n")
        for d in bad:
            log.write(f"DIFFERS\t{d}\n")
        for c in count_diff:
            log.write(f"COUNT\t{c}\n")
        log.write("The old folder was only read: nothing in it was moved, changed or deleted.\n")
    print(result)
    for d, e in failed[:10]:
        print(f"  FAILED {d}: {e}")
    for c in count_diff:
        print(f"  COUNT {c}")
    print(f"log: {log_path}")
    sys.exit(1 if failed or bad or count_diff else 0)


if __name__ == "__main__":
    main()
