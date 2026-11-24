#!/bin/sh
# 2026-11-02: the reference playthrough's PC replay of one live sitting (the checks, steps 3-6): a fresh portable folder from the reference playthrough's
# own exe and DLLs, the sitting's start card, the accurate preset, its recording as a pad script with the recorded clock
# start, full-speed replay clock, observe-only logs and the replay's own roll record (to compare with the live .rolls).
# Moved into the repo from a session scratch folder on 2026-11-11 (it runs run3_runner.py from this same folder).
#   sh tools/local/testing/run3_pc_replay.sh <sitting dir under decomp/_run3> <pass name> [extra env assignments...]
# e.g. sh tools/local/testing/run3_pc_replay.sh P01_20261102_204956 A
#      sh tools/local/testing/run3_pc_replay.sh P01_20261102_204956 vk ORRE_DUMP_FRAMES=100,200
# Options (environment): RUN_FROM, EXE_FROM, CHECK_EXE, CACHE_FROM, SEED_FROM, KEEP_CACHE, UI_FROM, GPU_BACKEND, ACCURATE_VIA
# (below); RUN3_TAIL / RUN3_STOP / RUN3_WRAP (run3_runner.py).
# RUN_FROM=<folder> (2026-11-24, the ORRE_ROOT test switch; the way for every sweep from now on): run the program in that
# frozen folder (made once per sweep by freeze_build.py: the exe, its DLLs, UI and fonts) with this sitting's data in
# its own check folder (ORRE_ROOT), instead of linking an exe and the DLLs into every check folder. One program location
# per sweep: no exe copy per run (the 5.5 clean house found the copies filling the disk) and one firewall entry at
# most. Needs a build with ORRE_ROOT (2026-11-24 on); the reference playthrough's own frozen exe and older ones don't read it, so they keep
# the old way (EXE_FROM, or the default). EXE_FROM, CHECK_EXE, UI_FROM and SEED_FROM are ignored with RUN_FROM (the
# frozen folder has its own UI and pipeline seed).
HERE=$(cd "$(dirname "$0")" && pwd)
# Folders, worked out from this script's place instead of one PC's absolute paths (2026-11-24, for the public repo; on
# this PC they come out the same as before): ORRE_REPO = the repo (default: three levels up from here); ORRE_WORK = the
# sittings and check folders (default: _run3 beside the repo); ORRE_EXTERNAL = third-party downloads kept outside the
# repo (default: _external beside it); RUN3_DIR = the recorded sittings' own folder (default: Desktop/OrrePort/Final/Run3
# in the home folder). POSIX shell paths: Windows through Git Bash / MSYS2 (cygpath below), the same on Linux / macOS
# minus cygpath (likely).
ROOT=${ORRE_REPO:-$(cd "$HERE/../../.." && pwd)}
WORK=${ORRE_WORK:-$(cd "$ROOT/.." && pwd)/_run3}
EXT=${ORRE_EXTERNAL:-$(cd "$ROOT/.." && pwd)/_external}
cd "$ROOT" || exit 1
export PATH="/c/msys64/mingw64/bin:$PATH"
SIT=$1; PASS=$2; shift 2
# *Added 2026-11-18:* P02 was recorded before port-gaps 60 fixed the P*DA cry screen, so it replays only with
# the old behaviour (ORRE_REPLAY_COMPAT=cry60, docs/port/hook-points.md OrreHook_OldCryIndex). uber_full.sh set it, this
# script didn't, and a census run without it split at frame 38,400 (the Strategy Memo made 10 Pokémon a frame instead
# of 9). Set here for P02 unless the caller set ORRE_REPLAY_COMPAT itself. Only P02 needs it (per hook-points.md).
case "$SIT" in P02_20261103_113532) export ORRE_REPLAY_COMPAT="${ORRE_REPLAY_COMPAT:-cry60}";; esac
SRC=$WORK/$SIT
OUT=$WORK/checks/${SIT}_$PASS
RUN3=${RUN3_DIR:-$HOME/Desktop/OrrePort/Final/Run3}
[ -e "$OUT" ] && { echo "$OUT exists: give a new pass name"; exit 1; }
mkdir -p "$OUT/Colosseum/ROM" "$OUT/Colosseum/Saves/Slot 1" "$OUT/Colosseum/Recordings" "$OUT/user"
# CHECK_EXE=1: the check build (build-pc/firstlink: the reference playthrough's source plus observe-only check features) instead of the reference playthrough's
# frozen exe; the DLLs and MinGW runtime still from the reference playthrough's folder. Its roll record must match the live one.
# hard links, not copies (snatcherLeo's disk cleanup, 2026-11-09: 426 runs had piled up 55 GB of exe and DLL copies); a
# plain copy only where a link can't be made
lnc() { ln "$1" "$2" 2>/dev/null || cp "$1" "$2"; }
# CACHE_FROM=<folder>: start from that folder's GPU pipeline cache (as a live sitting starts from the earlier ones'; the
# cache is the run's data, so it goes in the check folder either way)
[ -n "$CACHE_FROM" ] && cp -r "$CACHE_FROM" "$OUT/cache"
if [ -n "$RUN_FROM" ]; then
  [ -f "$RUN_FROM/orrecolo.exe" ] || { echo "RUN_FROM: no orrecolo.exe in $RUN_FROM (make it with freeze_build.py)"; exit 1; }
else
for d in $RUN3/*.dll; do lnc "$d" "$OUT/$(basename "$d")"; done
# SEED_FROM=<initial_pipeline_cache.db>: the bundled pipeline seed next to the exe (tools/local/port/pipeline_seed.py)
[ -n "$SEED_FROM" ] && cp "$SEED_FROM" "$OUT/initial_pipeline_cache.db"
# EXE_FROM=<path>: that exe instead (an older the reference playthrough exe from _run3/exes, for before / after comparisons)
if [ -n "$EXE_FROM" ]; then lnc "$EXE_FROM" "$OUT/orrecolo.exe";
elif [ -n "$CHECK_EXE" ]; then cp build-pc/firstlink/orrecolo.exe "$OUT/"; else lnc $RUN3/orrecolo.exe "$OUT/orrecolo.exe"; fi
# (CHECK_EXE stays a copy: build-pc/firstlink is relinked in place, so a link would change under a finished run)
# UI_FROM=<folder>: the RmlUi ui\ folder (settings.rml / .rcss), copied next to the exe; the hub builds need it
# (e.g. UI_FROM=<a worktree>/port/ui)
[ -n "$UI_FROM" ] && cp -r "$UI_FROM" "$OUT/ui" && mkdir -p "$OUT/ui/fonts" && \
  cp "$EXT"/fonts/inter/* "$EXT/fonts/saira/Saira-Variable.ttf" "$OUT/ui/fonts/" && \
  cp "$EXT/fonts/saira/OFL.txt" "$OUT/ui/fonts/OFL-Saira.txt" && \
  mv "$OUT/ui/fonts/OFL.txt" "$OUT/ui/fonts/OFL-Inter.txt"
fi # (RUN_FROM: nothing of the program's is put in the check folder)
# The disc: a hard link, so a pass never copies 1.4 GB per sitting. *Fixed 2026-11-17:* NTFS allows at most
# 1,023 hard links per file, and the check folders had used them all up on orig/'s image ("Too many links"). So the
# link comes from the first copy under _run3/discs/ (the project's scratch, beside the check folders; never snatcherLeo's own the reference playthrough
# folder) that still has room; when every copy is full (or none exists) a new copy is made from orig/ (read only,
# never changed). Windows / NTFS only in practice; on Linux / macOS ext4 and APFS allow far more links, so the first
# copy simply keeps serving (likely).
DISC="$OUT/Colosseum/ROM/Pokemon Colosseum (USA).iso"
DISCS=$WORK/discs
if ! ln "orig/Pokemon Colosseum (USA).iso" "$DISC" 2>/dev/null; then
  mkdir -p "$DISCS"
  linked=
  for c in "$DISCS"/disc_*.iso; do
    [ -f "$c" ] && ln "$c" "$DISC" 2>/dev/null && { linked=1; break; }
  done
  if [ -z "$linked" ]; then
    n=$(ls "$DISCS"/disc_*.iso 2>/dev/null | wc -l)
    cp "orig/Pokemon Colosseum (USA).iso" "$DISCS/disc_$n.iso" && ln "$DISCS/disc_$n.iso" "$DISC"
  fi
fi
cp $RUN3/Colosseum/disc-check.txt "$OUT/Colosseum/" 2>/dev/null
[ -f "$SRC/start_save.gci" ] && cp "$SRC/start_save.gci" "$OUT/Colosseum/Saves/Slot 1/01-GC6E-pokemon_colosseum.gci"
printf '# Run 3 check replay (%s): the sitting'"'"'s preset\npreset = accurate\n%s' "$PASS" "${GPU_BACKEND:+gpu_backend = $GPU_BACKEND
}" > "$OUT/user/settings.ini"
# ACCURATE_VIA=save (2.12, 2026-11-16): accurate mode through the save's own setting instead of the program-wide preset,
# as players get it since accurate mode became per save: settings.ini has no preset line, and Slot 1's setup file says
# gamecube_accurate=1 (pc_options.c decides the mode from it). Without it, the old "preset = accurate" line above still
# makes Slot 1 accurate through the one-time migration (pc_saves.c), so older passes replay the same.
if [ "$ACCURATE_VIA" = save ]; then
  printf '# Run 3 check replay (%s): accurate mode from the save'"'"'s own setting (Slot 1 slot.ini)\n%s' "$PASS" \
    "${GPU_BACKEND:+gpu_backend = $GPU_BACKEND
}" > "$OUT/user/settings.ini"
  printf '# Run 3 check replay: the sitting'"'"'s save, set to GameCube-accurate mode\nversion=1\nname=Run 3\ncame_from=imported\nnew=0\norder=1\ngamecube_accurate=1\n' \
    > "$OUT/Colosseum/Saves/Slot 1/slot.ini"
fi
# ACCURATE_VIA=off (2026-11-24, step 5.7: "with GameCube-accurate mode on and off"): the save in normal mode, every
# setting at its default (the tweaks that are on by default stay on), as a player who never touches accurate mode
# gets it. Its rolls are compared with the accurate baseline; differences are expected only where a default tweak
# changes timing or behaviour, and each one has to be explained.
if [ "$ACCURATE_VIA" = off ]; then
  printf '# Run 3 check replay (%s): normal mode, every setting at its default\n%s' "$PASS" \
    "${GPU_BACKEND:+gpu_backend = $GPU_BACKEND
}" > "$OUT/user/settings.ini"
  printf '# Run 3 check replay: the sitting'"'"'s save in normal mode\nversion=1\nname=Run 3\ncame_from=imported\nnew=0\norder=1\ngamecube_accurate=0\n' \
    > "$OUT/Colosseum/Saves/Slot 1/slot.ini"
fi
REC=$(ls $SRC/*.orrepad | head -1)
EPOCH=$(python -I tools/local/port/orrepad_to_pad.py "$REC" --info | sed -n 's/.*ORRE_VIRTUAL_EPOCH=\([0-9]*\).*/\1/p')
python -I tools/local/port/orrepad_to_pad.py "$REC" "$OUT/sitting.pad" | tail -1
W=$(cygpath -m "$OUT")
cd "$OUT" || exit 1
# RUN_FROM: the frozen program runs with this check folder as its OrrePort folder (ORRE_ROOT, pc_paths.c), and the
# runner starts that program (RUN3_EXE); its DLLs sit beside it, so the program's own folder goes first on PATH
if [ -n "$RUN_FROM" ]; then
  export ORRE_ROOT="$W" RUN3_EXE="$(cygpath -m "$RUN_FROM")/orrecolo.exe" PATH="$RUN_FROM:$PATH"
fi
env ORRE_WINDOW=minimized ORRE_REPLAY_SPEED=max ORRE_VIRTUAL_EPOCH=$EPOCH ORRE_PAD_SCRIPT=@file:$W/sitting.pad \
    ORRE_ROLL_RECORD=$W/replay.rolls ORRE_SLICE_LOG=1 ORRE_BATTLE_LOG=1 ORRE_EVENT_LOG=1 ORRE_CAMPARAMS_LOG=1 \
    ORRE_SOUND_LOG=1 ORRE_SFX_LOG=1 ORRE_MSG_LOG=1 ORRE_MENU_LOG=1 ORRE_RUN_WHY="Main: Run 3 $SIT, PC replay pass $PASS" "$@" \
    python -I "$ROOT/tools/local/saferun.py" --exclusive --time 2400 \
    --why "Run 3 check: $SIT replayed on the PC (pass $PASS)" -- python -I $(cygpath -m "$HERE/run3_runner.py") "$W"
echo "rc=$?"
echo "== pad script: $(grep -m1 'pad_script\] [0-9]* steps' run.log | cut -c1-120)"
# the "field" list (2026-11-03) is new: set aside on both sides, so older live recordings still compare
grep -v '^field' replay.rolls | cut -d' ' -f1-4 > replay.rolls.cmp; grep -v '^field' $(ls $SRC/*.rolls | head -1) | cut -d' ' -f1-4 > live.rolls.cmp
echo "== field lines in the replay: $(grep -c '^field' replay.rolls)"
echo "== rolls: live $(wc -l < live.rolls.cmp), replay $(wc -l < replay.rolls.cmp); $(cmp -s replay.rolls.cmp live.rolls.cmp && echo IDENTICAL || echo "DIFFER from line $(cmp replay.rolls.cmp live.rolls.cmp | sed -n 's/.*line \([0-9]*\).*/\1/p')")"
grep '^\[room\] request' $(ls $SRC/*.log | head -1) > live.rooms; grep '^\[room\] request' run.log > replay.rooms
echo "== room requests: live $(wc -l < live.rooms), replay $(wc -l < replay.rooms); $(cmp -s live.rooms replay.rooms && echo IDENTICAL || echo DIFFER)"
tail -1 run.log
# the run's GPU shader cache is rebuilt by any later run: drop it (KEEP_CACHE=1 keeps it, e.g. to seed a warm replay)
[ -z "$KEEP_CACHE" ] && rm -rf -- "$OUT/cache"
