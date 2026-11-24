#!/bin/sh
# The full the reference playthrough replay check (step 5.7, 2026-11-24): every the reference playthrough sitting replayed from ONE frozen build, for each
# configuration given, each result compared with the base2 baseline (_run3/checks/<sitting>_base2: rolls and room
# requests). Why: before the playtest release, prove the build plays every recorded sitting the same way on both
# graphics backends and in both modes, without anyone watching (snatcherLeo, 2026-11-23: "During that time, I can go do
# somethign else").
#
#   sh tools/local/testing/full_replay_check.sh <frozen build folder> <tag> <config> [<config> ...]
#   config = <backend>:<mode>, backend vulkan | d3d12, mode acc (GameCube-accurate, from the save) | off (normal mode,
#   defaults). e.g. sh tools/local/testing/full_replay_check.sh "$FROZEN" r57 vulkan:acc d3d12:acc vulkan:off d3d12:off
#
# One replay at a time (run3_pc_replay.sh runs each through saferun --exclusive: PC-port comparisons run alone), each
# sitting to its own end; run3_runner.py's stall check ends a run that stops making frames, saferun's time cap is the
# backstop. A summary line per run goes to <work>/checks/full_<tag>.summary, written as each run ends, so a stopped
# sweep keeps its results. Saferun waits on its own while the player's launcher is running (no runs while snatcherLeo
# plays).
# Platforms: POSIX shell (Git Bash / MSYS2 on Windows); the same on Linux / macOS with their backends (likely).
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=${ORRE_REPO:-$(cd "$HERE/../../.." && pwd)}
WORK=${ORRE_WORK:-$(cd "$ROOT/.." && pwd)/_run3}
FROZEN=$1; TAG=$2; shift 2
[ -f "$FROZEN/orrecolo.exe" ] || { echo "no orrecolo.exe in $FROZEN (make it with freeze_build.py)"; exit 1; }
# the sittings with a base2 baseline (P09 and P12 are prefix checks of their live recordings, as in base2)
SITS=${SITS:-$(cd "$WORK/checks" && ls -d *_base2 | sed 's/_base2$//')}
SUM="$WORK/checks/full_$TAG.summary"
echo "# full replay check $TAG from $FROZEN, started $(date '+%F %T')" >> "$SUM"
for cfg in "$@"; do
  BE=${cfg%%:*}; MODE=${cfg#*:}
  case "$MODE" in acc) VIA=save;; off) VIA=off;; *) echo "mode $MODE: acc or off"; exit 1;; esac
  for s in $SITS; do
    PASS="${TAG}_${BE}_${MODE}"
    OUT="$WORK/checks/${s}_$PASS"
    [ -e "$OUT/replay.rolls" ] && { echo "$s $PASS: already done"; continue; }
    T0=$(date +%s)
    RUN_FROM="$FROZEN" GPU_BACKEND="$BE" ACCURATE_VIA="$VIA" sh "$HERE/run3_pc_replay.sh" "$s" "$PASS" > /dev/null 2>&1
    B="$WORK/checks/${s}_base2"
    r=$(cmp -s "$OUT/replay.rolls" "$B/replay.rolls" && echo same || echo DIFFER)
    m=$(cmp -s "$OUT/replay.rooms" "$B/replay.rooms" && echo same || echo DIFFER)
    end=$(tail -1 "$OUT/run.log" 2>/dev/null | cut -c1-80)
    line="$s $BE $MODE: rolls $r, rooms $m vs base2; $(( $(date +%s) - T0 ))s; last log line: $end"
    echo "$line" >> "$SUM"
    echo "$line"
  done
done
echo "# finished $(date '+%F %T')" >> "$SUM"
