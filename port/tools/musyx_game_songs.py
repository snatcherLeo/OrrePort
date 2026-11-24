"""List the game's music by its own sound ids, and render each song the way the game starts it.

How the game starts music (docs/engine/music.md, "Sound ids, the sound table and the song table"):
  sound id -> sound table entry (common_rel, 12 bytes; GSsndEntry): +0x00 flags (bit 7 = resident effect, not music),
              +0x01 MusyX group, +0x02 reverb preset (0 = keep the current one), +0x03 song-table index,
              +0x04 MusyX song id (u16)
  song-table entry (common_rel, 8 bytes; GSsndWaveSlot) -> first word = the song's entry id in bgm_archive.fsys
  reverb preset (common_rel, 24 bytes; lbl_80478FA4): coloration, mix, time, damping, pre-delay, cross-talk
              (_sndSetReverbParm, gs_dvd.c), applied with sndAuxCallbackUpdateSettingsReverbHI before the song starts
  then sndSeqPlayEx(group, song id, song data, params) (gs_dvd.c fn_8016761C).
MusyX group 0 is the snd_music bank, group 1 the snd_music_atmos bank.

Usage (pc-platform repo root):
  python -I port/tools/musyx_game_songs.py --extracted <extracted dir> [--orig <dir with common.fsys, bgm_archive.fsys>]
      lists every music sound id (stdout). The tables come from the archives in --orig when given, otherwise from
      the extracted copies (data_tables/original/common_rel.rel; the song entry ids in
      audio/songs/original/README.md)
  ... --render <out dir> [--seconds N] [--bin build-pc] [--only <song name>]
      renders each distinct (song, group, song id, reverb) with musyx_play_ours (our runtime) and musyx_play
      (AxioDL's), and reports whether the two are byte-identical. WAVs go to <out dir> (use build-pc/generated/...:
      game data, never committed).
"""
import argparse
import hashlib
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "local"))
from colo.fsys import fsys_entries  # noqa: E402
from colo.tables import table_rows  # noqa: E402

SOUND_TABLE = (0x13FE08, 0x14A6EC, 12)  # section-4 offset, count offset, entry size (colo/tables.py)
SONG_TABLE = (0x111B8C, 0x14A6BC, 8)
REVERB_TABLE = (0x1437F8, 0x14A6F0, 24)  # 12 presets; found 2026-10-15 (count slot = 12, floats fit the 6 fields)
BANKS = {0: "music", 1: "music_atmos"}


def entries(path):
    return list(fsys_entries(open(path, "rb").read()))


def song_ids_from_readme(extracted):
    """entry id -> song name, from the '## `<name>.song`' sections and their '- entry id 0x...' lines"""
    songs, name = {}, None
    for line in open(os.path.join(extracted, "audio", "songs", "original", "README.md"), encoding="utf-8"):
        if line.startswith("## `") and line.rstrip().endswith(".song`"):
            name = line.strip()[4:-6]
        elif name and line.startswith("- entry id 0x"):
            songs[int(line.split()[3], 16)] = name
            name = None
    return songs


def load_tables(orig, extracted):
    if orig and os.path.exists(os.path.join(orig, "bgm_archive.fsys")):
        rel = next(load() for k, name, t, ident, load in entries(os.path.join(orig, "common.fsys"))
                   if name == "common_rel")
        songs = {ident: name for k, name, t, ident, load in entries(os.path.join(orig, "bgm_archive.fsys"))}
    else:
        rel = open(os.path.join(extracted, "data_tables", "original", "common_rel.rel"), "rb").read()
        songs = song_ids_from_readme(extracted)
    sound = table_rows(rel, *SOUND_TABLE)
    song_table = table_rows(rel, *SONG_TABLE)
    reverbs = [struct.unpack(">6f", e) for e in table_rows(rel, *REVERB_TABLE)]
    music = []
    for sid, e in enumerate(sound):
        flags, group, reverb, wave = e[0], e[1], e[2], e[3]
        song_id = struct.unpack(">H", e[4:6])[0]
        if sid == 0 or flags & 0x80 or wave >= len(song_table):
            continue
        ident = struct.unpack(">I", song_table[wave][:4])[0]
        name = songs.get(ident)
        if name is None:
            continue
        music.append(dict(sid=sid, group=group, song_id=song_id, reverb=reverb, wave=wave, song=name))
    return music, reverbs


def render(m, reverbs, args):
    bank = BANKS.get(m["group"])
    if bank is None:
        return f"group {m['group']}: no bank known"
    data = os.path.join(args.extracted, "audio")
    proj = os.path.join(data, "musyx_data", "original", f"snd_{bank}_proj.bin")
    pool = os.path.join(data, "musyx_data", "original", f"snd_{bank}_pool.bin")
    sdir = os.path.join(data, "musyx_data", "original", f"snd_{bank}_sdir.bin")
    samp = os.path.join(data, bank, "original", f"snd_{bank}.samp")
    song = os.path.join(data, "songs", "original", m["song"] + ".song")
    env = dict(os.environ)
    if m["reverb"] != 0 and m["reverb"] < len(reverbs):
        env["MUSYX_PLAY_REVERB"] = ",".join(repr(v) for v in reverbs[m["reverb"]])
    else:
        env.pop("MUSYX_PLAY_REVERB", None)
    tag = f"{m['song']}_g{m['group']}_s{m['song_id']}_r{m['reverb']}"
    digests = []
    for exe in ("musyx_play_ours", "musyx_play"):
        out = os.path.join(args.render, f"{tag}_{exe}.wav")
        cmd = [os.path.join(args.bin, exe + ".exe" if os.name == "nt" else exe), proj, pool, sdir, samp, out, "song", song,
               str(m["song_id"]), str(args.seconds)]
        r = subprocess.run(cmd, env=env, capture_output=True, text=True)
        silent = r.returncode == 1 and "(SILENT)" in r.stdout  # musyx_play's exit code 1 = rendered, but silent
        if (r.returncode != 0 and not silent) or not os.path.exists(out):
            return f"{exe} failed ({r.returncode}): {(r.stderr or r.stdout).strip()[-200:]}"
        digests.append(hashlib.sha1(open(out, "rb").read()).hexdigest())
    pcm = open(os.path.join(args.render, f"{tag}_musyx_play_ours.wav"), "rb").read()[44:]
    peak = max((abs(v) for v in struct.unpack(f"<{len(pcm) // 2}h", pcm)), default=0)
    return ("identical" if digests[0] == digests[1] else "DIFFERENT") + f", peak {peak}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig")
    ap.add_argument("--extracted", required=True)
    ap.add_argument("--render")
    ap.add_argument("--seconds", type=int, default=5)
    ap.add_argument("--bin", default="build-pc")
    ap.add_argument("--only")
    args = ap.parse_args()

    music, reverbs = load_tables(args.orig, args.extracted)
    print(f"{len(music)} music sound ids; {len(reverbs)} reverb presets")
    for i, r in enumerate(reverbs):
        print(f"  reverb {i:2d}: coloration {r[0]:.3g}, mix {r[1]:.3g}, time {r[2]:.3g} s, damping {r[3]:.3g}, "
              f"pre-delay {r[4] * 1000:.0f} ms, cross-talk {r[5]:.3g}")
    if args.render:
        os.makedirs(args.render, exist_ok=True)
    done = {}
    same = diff = 0
    for m in music:
        line = (f"sound {m['sid']:4d}: {m['song']:24s} group {m['group']} song id {m['song_id']:3d} "
                f"reverb {m['reverb']:2d}")
        if args.render and (args.only is None or args.only == m["song"]):
            key = (m["song"], m["group"], m["song_id"], m["reverb"])
            if key not in done:
                done[key] = render(m, reverbs, args)
                same += done[key].startswith("identical")
                diff += not done[key].startswith("identical")
            line += f"  -> {done[key]}"
        print(line)
    if args.render:
        print(f"rendered {len(done)} distinct songs: {same} identical on both runtimes, {diff} not")
        return 0 if diff == 0 else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
